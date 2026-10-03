#include "../cell_manager.h"
#include <algorithm>
#include <cstdint>
#include <entities/body.h>
#include <entities/cell/cell.h>
#include <entities/cell/cell_settings.h>
#include <entities/matter/CellMatter.h>
#include <entities/matter/matter_settings.h>
#include <entities/spring/spring.h>
#include <iostream>
#include <managers/cell_manager/cell_manager_settings.h>
#include <ostream>
#include <SFML/System/Vector2.hpp>
#include <simulation/context/sim_command.h>
#include <unordered_map>
#include <Utils/o_vec/o_vector.hpp>
#include <Utils/random.h>
#include <Utils/spatial_grid/fixed_span.h>
#include <Utils/spatial_grid/simple_spatial_grid.h>

template<typename T>
void CellManager::gather_objects_in_radius(FixedSpan<cell_idx, uint16_t>& indexes, const o_vector<T>& objects, const sf::Vector2f& position, const float radius)
{
	indexes.clear();

	for (T* object : objects)
	{
		Body* body = bodies_->at(object->body_id_);
		float dist_sq = (body->position_ - position).lengthSquared();

		if (dist_sq < radius * radius)
			indexes.add(object->id_);
	}
}

void CellManager::remove_cells_in_radius(const sf::Vector2f& position, const float radius)
{
	gather_objects_in_radius(selected_cells_indexes_, all_cells_, position, radius);
	gather_objects_in_radius(selected_matter_indexes_, all_cell_matter_, position, radius);

	for (int i = 0; i < selected_cells_indexes_.count; ++i)
		remove_cell(selected_cells_indexes_[i]);
	for (int i = 0; i < selected_matter_indexes_.count; ++i)
		remove_cell_matter(selected_matter_indexes_[i]);

	check_for_dangling_springs();
}

void CellManager::check_for_dangling_springs()
{
	std::cout << "cell manager events.cpp: check_for_dangling_springs() called, checking for dangling springs..." << std::endl;
	/* Check for springs that are connected to cells that have been removed */
	for (Spring* spring : all_springs_)
	{
		Cell* cell_a = all_cells_.at(spring->cell_A_id);
		Cell* cell_b = all_cells_.at(spring->cell_B_id);
		if (all_cells_.is_obj_active(cell_a->id_) == false || all_cells_.is_obj_active(cell_b->id_) == false)
		{
			std::cout << "cell manager events.cpp: found dangling spring, removing..." << std::endl;
			all_springs_.remove(spring);
		}
	}
}

void CellManager::influence_cell_velocities_in_radii(const sf::Vector2f& position, const float radius, const float intensity)
{
	/* This function changes the velocity of cells and cell matter in a given radius */
	gather_objects_in_radius(selected_cells_indexes_, all_cells_, position, radius);
	gather_objects_in_radius(selected_matter_indexes_, all_cell_matter_, position, radius);

	for (int i = 0; i < selected_cells_indexes_.count; ++i)
	{
		Cell* cell = all_cells_.at(selected_cells_indexes_[i]);
		Body* body = bodies_->at(cell->body_id_);

		sf::Vector2f direction = (position - body->position_).normalized();
		body->velocity_ += direction * intensity;
	}

	for (int i = 0; i < selected_matter_indexes_.count; ++i)
	{
		CellMatter* matter = all_cell_matter_.at(selected_matter_indexes_[i]);
		Body* body = bodies_->at(matter->body_id_);

		sf::Vector2f direction = (position - body->position_).normalized();
		body->velocity_ += direction * (float)intensity;
	}
}

void CellManager::handle_cell_manager_event(SimCommand& cmd)
{
	switch (cmd.type)
	{
	case CommandType::SetCellToggles:
		toggles_ = cmd.cell_toggles;
		break;

		// Spring Natual Selection Modifier
	case CommandType::SetSpringBreakingForce:
		Spring::spring_break_force = cmd.float_val;
		break;
	case CommandType::SetSpringBreakingLength:
		Spring::spring_break_length_factor = cmd.float_val;
		break;
	case CommandType::SetSpringDamageThreshold:
		Spring::spring_damage_threshold = cmd.float_val;
		break;
	case CommandType::SetSpringWorkConst:
		Spring::spring_work_const = cmd.float_val;
		break;

	case CommandType::SetMinSpeed:
		statistics_.min_speed = cmd.float_val;
		break;

	case CommandType::SetRadius:
		//    if (selected_protozoa)
		//        selected_protozoa->get_cells()[cmd.cell_spring_idx].radius = cmd.float_val;
		break;

	case CommandType::SetAmplitude:
		//    if (selected_protozoa)
		//        selected_protozoa->get_cells()[cmd.cell_spring_idx].amplitude = cmd.float_val;
		break;

	case CommandType::SetFrequency:
		//if (selected_protozoa)
		//    selected_protozoa->get_cells()[cmd.cell_spring_idx].frequency = cmd.float_val;
		break;

	case CommandType::SetVerticalShift:
		//if (selected_protozoa)
		//    selected_protozoa->get_cells()[cmd.cell_spring_idx].vertical_shift = cmd.float_val;
		break;

	case CommandType::SetOffset:
		//if (selected_protozoa)
		//    selected_protozoa->get_cells()[cmd.cell_spring_idx].offset = cmd.float_val;
		break;

	case CommandType::MutateProtozoa:
		mutate_selected_protozoa();
		break;

	case CommandType::AddCell:
		if (protozoa_tracker_.is_active == true)
			create_weak_offspring(protozoa_tracker_.original_selected_cell_id);
		break;

	case CommandType::RemoveCell:
		if (protozoa_tracker_.is_active == true)
			remove_cell(protozoa_tracker_.original_selected_cell_id);
		break;

	case CommandType::AddSpring:
		//if (selected_protozoa)
		//    selected_protozoa->add_spring(); todo
		break;

	case CommandType::RemoveSpring:
		//if (selected_protozoa)
		//    selected_protozoa->remove_spring(); todo
		break;

	case CommandType::InjectProtozoa:
		inject_selected_protozoa(cmd.bool_val, cmd.float_val);
		break;

	case CommandType::KillProtozoa:
		kill_selected_protozoa();
		break;

	case CommandType::ForceReproduce:
		force_reproduce_selected_protozoa();
		break;

	case CommandType::MakeImmortal:
		if (protozoa_tracker_.is_active == true)
			all_cells_.at(protozoa_tracker_.original_selected_cell_id)->immortal_ = true;
		break;

	case CommandType::CloneProtozoa:
		clone_selected_protozoa();
		break;

	case CommandType::SetImpulseDamageMultiplier:
		CellManagerSettings::impulse_damage_multiplier = cmd.float_val;
		break;

	case CommandType::SetImpulseDamageThreshold:
		CellManagerSettings::impulse_damage_thresh = cmd.float_val;
		break;

	case CommandType::SetMaxImpulseDamage:
		CellManagerSettings::max_single_hit_integrity_fraction = cmd.float_val;
		break;

	case CommandType::SetCellMatterLifetime:
		CellMatterSettings::max_time_to_live = cmd.int_val;
		break;

	case CommandType::SetSpringAmplitude:
		//if (selected_protozoa)
		//{
		//    m_world_.selected_protozoa_->get_springs()[cmd.cell_spring_idx].amplitude = cmd.float_val;
		//}
		break;

	case CommandType::SetSpringFrequency:
		//if (selected_protozoa)
		//{
		//    m_world_.selected_protozoa_->get_springs()[cmd.cell_spring_idx].frequency = cmd.float_val;
		//}
		break;

	case CommandType::SetSpringOffset:
		//if (selected_protozoa)
		//{
		//    m_world_.selected_protozoa_->get_springs()[cmd.cell_spring_idx].offset = cmd.float_val;
		//}
		//break;
			//m_world_.selected_protozoa_->get_springs()[cmd.cell_spring_idx].offset = cmd.float_val;
		//}
		break;

	case CommandType::SetSpringVerticalShift:
		//if (selected_protozoa)
		//{
			//m_world_.selected_protozoa_->get_springs()[cmd.cell_spring_idx].vertical_shift = cmd.float_val;
		//}
		break;

	case CommandType::SetDampingConst:
		//if (selected_protozoa)
		//{
		   // m_world_.selected_protozoa_->get_springs()[cmd.cell_spring_idx].damping = cmd.float_val;
		//}
		break;

	case CommandType::SetSpringConst:
		//if (selected_protozoa)
		//{
		   // m_world_.selected_protozoa_->get_springs()[cmd.cell_spring_idx].spring_const = cmd.float_val;
		//}
		break;

	}
}

void CellManager::mutate_selected_protozoa()
{
	if (selected_cell_id_ == -1 || protozoa_tracker_.is_active == false)
		return;

	for (Cell& fake_cell : protozoa_tracker_.cells)
	{
		Cell* cell = all_cells_.at(fake_cell.id_);
		cell->mutate();
	}
}

void CellManager::inject_selected_protozoa(bool is_energy, float amount)
{
	if (selected_cell_id_ == -1 || protozoa_tracker_.is_active == false)
		return;

	const float per_cell = amount / protozoa_tracker_.cells.size();

	for (Cell& fake_cell : protozoa_tracker_.cells)
	{
		Cell* cell = all_cells_.at(fake_cell.id_);

		if (is_energy)
			cell->change_energy(per_cell);
		else
			cell->nutrients_ += per_cell;
	}
}

void CellManager::kill_selected_protozoa()
{
	// every cell and spring in the current selected protozoa is removed from the world
	if (selected_cell_id_ == -1 || protozoa_tracker_.is_active == false)
		return;

	for (Cell& cell : protozoa_tracker_.cells)
	{
		remove_cell(cell.id_);
	}

	for (Spring& spring : protozoa_tracker_.springs)
	{
		all_springs_.remove(spring.id_);
	}
}

void CellManager::force_reproduce_selected_protozoa()
{
	// every cell in the current selected protozoa is forced to reproduce by making their nutrients, energy, and integrity reach
	// their minimum reproduction thresholds
	if (selected_cell_id_ == -1 || protozoa_tracker_.is_active == false)
		return;

	for (Cell& fake_cell : protozoa_tracker_.cells)
	{
		Cell* cell = all_cells_.at(fake_cell.id_);
		cell->nutrients_ = std::max(cell->nutrients_, cell->birth_nutrients_thresh * CellSettings::max_nutrients);

		cell->set_energy(std::max(cell->get_energy(), cell->birth_energy_thresh * CellSettings::max_energy));
		cell->set_integrity(std::max(cell->get_integrity(), cell->birth_integrity_thresh * CellSettings::max_integrity));

		cell->repro_timer_ = std::max(cell->repro_timer_, uint16_t(cell->repro_cooldown));
		cell->force_reproduce();
	}
}


void CellManager::clone_selected_protozoa()
{
	if (selected_cell_id_ == -1 || protozoa_tracker_.is_active == false)
		return;

	sf::Vector2f parent_center = protozoa_tracker_.position;
	sf::Vector2f child_spawn_pos = Random::rand_position_in_circle(parent_center, 100.f);
	float child_spawn_rad = 60.f;

	// maps the parent's absolute cell id -> the corresponding clone's absolute cell id
	std::unordered_map<int32_t, int32_t> old_to_new_cell_id;
	old_to_new_cell_id.reserve(protozoa_tracker_.cells.size());

	for (Cell& cell : protozoa_tracker_.cells)
	{

		CellBodyPair cpair = create_cell({ 0, 0 }, false);

		if (!cpair.is_valid)
		{
			std::cerr << "[ERROR]: Failed to create cell during protozoa cloning. Max cells reached.\n";
			return;
		}

		const Body* parent_body = bodies_->at(cell.body_id_);

		Cell* new_cell = all_cells_.at(cpair.cell_id);
		Body* new_body = bodies_->at(cpair.body_id);

		new_cell->copy_genetics(cell);
		new_body->copy(parent_body);
		new_body->position_ = Random::rand_position_in_circle(child_spawn_pos, child_spawn_rad);

		old_to_new_cell_id[cell.id_] = cpair.cell_id;
	}

	for (Spring& spring : protozoa_tracker_.springs)
	{
		auto it_a = old_to_new_cell_id.find(spring.cell_A_id);
		auto it_b = old_to_new_cell_id.find(spring.cell_B_id);

		if (it_a == old_to_new_cell_id.end() || it_b == old_to_new_cell_id.end())
		{
			std::cerr << "[ERROR]: Spring references a cell not present in the cloned set.\n";
			continue;
		}

		int32_t spring_id = create_spring(it_a->second, it_b->second);

		if (spring_id == -1)
		{
			std::cerr << "[ERROR]: Failed to create spring during protozoa cloning. Max springs reached.\n";
			return;
		}

		Spring* new_spring = all_springs_.at(spring_id);

		new_spring->genome.copy_genetics(spring.genome);
	}
}