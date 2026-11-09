#include "../cell_manager.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <entities/body/body.h>
#include <entities/cell/cell.h>
#include <entities/matter/CellMatter.h>
#include <entities/spring/tendon.h>
#include <iostream>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Mouse.hpp>
#include <simulation/context/sim_snapshot.h>
#include <simulation/context/state.h>
#include <Utils/spatial_grid/simple_spatial_grid.h>

bool CellManager::deselect_cell()
{
	if (selected_cell_id_ == -1)
		return false;
	selected_cell_id_ = -1;
	protozoa_tracker_.is_active = false;

	return true;
}


void CellManager::add_new_cells_to_grid()
{
	new_born_cell_grid_.clear();

	for (Cell* cell : all_cells_)
	{
		bool is_newborn = cell->internal_clock_ < CellSettings::infant_time;
		bool is_first_gen = cell->get_generation() == 0; // we dont want the initial cells during the sim start to all tangle
		if (!is_newborn || is_first_gen)
			continue;

		Body* body = bodies_->at(cell->body_id_);
		const sf::Vector2f pos = body->get_pos();
		new_born_cell_grid_.add_object(pos.x, pos.y, cell->id_);
	}
}


void CellManager::update(int iterations)
{
	if (extinction_event)
		return;

	// this has to happen before we move about the cells
	add_new_cells_to_grid();

	// if we have a selected cell and it has died, we need to deselect it to avoid null errors
	if (selected_cell_id_ == -1 || !all_cells_.at(selected_cell_id_)->is_alive())
	{
		deselect_cell();
	}
	// NOTE:
	// the order MUST be reproduction -> death -> updating
	// if any bugs involving stray connections arise in the future, it is likely due to this order being changed

	// --------------- reproductive system ---------------
	collect_connection_requests(); // Connection Requests are collected here, but not applied yet
	apply_connection_requests();

	collect_reproduction_requests();
	apply_reproduction_requests();

	apply_matter_birth_requests();

	// --------------- death management ---------------
	collect_cell_death_requests();
	collect_matter_death_requests();
	collect_tendon_death_requests();

	apply_cell_death_requests();
	apply_matter_death_requests();
	apply_tendon_death_requests();

	// --------------- updating cells and matter ---------------
	spawn_immune = iterations < init_spring_immunity_time;

	update_tendons(spawn_immune);
	update_cells();
	update_cell_matter();

	check_for_extinction_event();

	// --------------- statistics ---------------
	update_statistics();
}

void CellManager::update_cells()
{
	current_total_cells_ = all_cells_.occupied_count;
	ensure_update_jobs_built();
	thread_pool_.run_and_wait();
}

void CellManager::update_cell_matter()
{
	for (CellMatter* matter : all_cell_matter_)
	{
		Body* body = bodies_->at(matter->body_id_);
		matter->update(body);
	}
}

void CellManager::update_cell(Cell* cell)
{
	Body* body = bodies_->at(cell->body_id_);
	cell->update_statistics();
	cell->update_organics(spawn_immune, toggles_.friction_energy_loss, toggles_.old_age_death);
	
	body->apply_drag(cell->cell_calculated_drag_);

	speed_tax_cell(cell);
	impulse_tax_cell(cell, body->impulse_);
}

void CellManager::impulse_tax_cell(Cell* cell, const float impulse) const
{
	/* Apply a tax to the cell based on the impulse it has received, sort of like a collision injury */
	if (impulse < impulse_damage_thresh || spawn_immune || !toggles_.collision_integrity_damage_)
		return;

	float damage = -(impulse - impulse_damage_thresh) * impulse_damage_multiplier;
	damage = std::max(damage, -Cell::max_integrity * max_single_hit_integrity_fraction);

	cell->change_integrity(damage);
	cell->cumulative_collision_damage_ += std::abs(damage);
}

void CellManager::collect_connection_requests()
{
	for (Cell* cell : all_cells_)
		if (cell->should_try_connect_to_others())
			try_connect_newborn_cell(cell);
}


void CellManager::update_position_container(RenderData& rend_data, const sf::FloatRect& visible_bounds, const bool show_only_newborns, const bool show_cells, const bool show_matter)
{
	rend_data.tendon_connections.clear();
	if (show_cells)
	{
		add_cells_to_render_data(rend_data, visible_bounds, show_only_newborns);
		add_tendons_to_render_data(rend_data, visible_bounds, show_only_newborns);
	}

	if (!show_only_newborns && show_matter)
		add_matter_to_render_data(rend_data, visible_bounds);
}

void CellManager::add_cells_to_render_data(RenderData& rend_data, const sf::FloatRect& visible_bounds, const bool show_only_newborns)
{
	for (const Cell* cell : all_cells_)
	{
		Body* body = bodies_->at(cell->body_id_);
		const sf::Vector2f pos = body->get_pos();
		const sf::Vector2f vel = body->get_vel();

		if (!visible_bounds.contains(pos))
			continue;

		if (show_only_newborns && cell->should_try_connect_to_others())
			continue;

		rend_data.outer_colors.push_back(cell->get_outer_color());
		rend_data.inner_colors.push_back(cell->get_inner_color());
		rend_data.positions.push_back(pos);
		rend_data.velocities.push_back(vel);
		rend_data.radii.push_back(body->get_radius());
	}
}

void CellManager::add_tendons_to_render_data(RenderData& rend_data, const sf::FloatRect& visible_bounds, const bool show_only_newborns)
{
	const int tendon_count = static_cast<int>(all_tendons_.size());

	for (Tendon* tendon : all_tendons_)
	{
		Cell* cell_a = all_cells_.at(tendon->cell_A_id);
		Body* body_a = bodies_->at(cell_a->body_id_);

		const sf::Vector2f pos_a = body_a->get_pos();
		const sf::Vector2f vel_a = body_a->get_vel();
		bool cell_a_visible = visible_bounds.contains(pos_a);
		bool cell_a_newborn = cell_a->internal_clock_ < CellSettings::infant_time;

		if (show_only_newborns && cell_a_newborn)
			continue;

		Cell* cell_b = all_cells_.at(tendon->cell_B_id);
		Body* body_b = bodies_->at(cell_b->body_id_);

		const sf::Vector2f pos_b = body_b->get_pos();
		const sf::Vector2f vel_b = body_b->get_vel();
		bool cell_b_visible = visible_bounds.contains(pos_b);
		bool cell_b_newborn = cell_b->internal_clock_ < CellSettings::infant_time;

		if (show_only_newborns && cell_b_newborn)
			continue;

		if (!cell_a_visible && !cell_b_visible)
			continue;

		const float min_dist = body_a->get_radius() + body_b->get_radius();

		// The reason why we cant use indexing to fill this array is because we dont know how many bodies are not active,
		// so it messes with the indexing and leads to null connections
		rend_data.tendon_connections.push_back({
			pos_a,
			pos_b,
			tendon->genome.outer_r, tendon->genome.outer_g, tendon->genome.outer_b,
			min_dist,
			min_dist * 3.f,
			tendon->stress });
	}
}

void CellManager::add_matter_to_render_data(RenderData& rend_data, const sf::FloatRect& visible_bounds)
{
	for (const CellMatter* matter : all_cell_matter_)
	{
		Body* body = bodies_->at(matter->body_id_);
		const sf::Vector2f pos = body->get_pos();
		const sf::Vector2f vel = body->get_vel();

		if (!visible_bounds.contains(pos))
			continue;

		rend_data.outer_colors.push_back(matter->outer_color());
		rend_data.inner_colors.push_back(matter->inner_color());
		rend_data.positions.push_back(pos);
		rend_data.velocities.push_back(vel);
		rend_data.radii.push_back(body->get_radius());
	}
}

// updating.cpp
void CellManager::try_connect_newborn_cell(Cell* cell)
{
	Body* body = bodies_->at(cell->body_id_);
	const sf::Vector2f pos = body->get_pos();

	nearby_cell_ids.clear();
	new_born_cell_grid_.find(pos.x, pos.y, &nearby_cell_ids);

	for (int i = 0; i < nearby_cell_ids.count; ++i)
	{
		cell_idx other_cell_id = nearby_cell_ids[i];
		if (other_cell_id == cell->id_)
			continue;

		// Canonical ordering: only the lower-id cell in a pair ever issues
		// the request. Both cells sit in the same grid and would otherwise
		// discover each other independently -> two requests, two tendons.
		if (cell->id_ > other_cell_id)
			continue;

		Cell* other_cell = all_cells_.at(other_cell_id);

		if (cell->already_connected_to(other_cell_id))
			continue;

		sf::Vector2f dir = pos - bodies_->at(other_cell->body_id_)->get_pos();
		float dist_sq = dir.lengthSquared();
		if (dist_sq < cell->newborn_search_radius * cell->newborn_search_radius)
		{
			connection_requests.push_back(ConnectionRequest{ (int32_t)cell->id_, (int32_t)other_cell->id_ });
			cell->register_connection(other_cell_id);
			other_cell->register_connection(cell->id_);
		}
	}
}


void CellManager::update_tendons(bool immune)
{
	for (Tendon* tendon : all_tendons_)
	{
		if (tendon->is_tendon_broken()) continue;

		Cell* cell_a = all_cells_.at(tendon->cell_A_id);
		Cell* cell_b = all_cells_.at(tendon->cell_B_id);

		bool true_immune = immune || !toggles_.tendon_stress_integrity_damage;
		tendon->update_organics(*cell_a, *cell_b, !true_immune, toggles_.work_done_energy);

		// otherwise we update the tendon physics and organics
		Body* body_a = bodies_->at(cell_a->body_id_);
		Body* body_b = bodies_->at(cell_b->body_id_);

		bool disable_length_breakage = immune || !toggles_.tendon_too_long_breakage;
		bool disable_force_breakage = immune || !toggles_.tendon_too_much_force_breakage;

		const float ma = body_a->mass_, mb = body_b->mass_;
		const float reduced_mass = (ma * mb) / (ma + mb);

		const physics_stats stats{
			.extension_fraction = tendon->extension_ratio,
			.spring_constant = tendon->get_spring_constant(),
			.damping = tendon->genome.damping,
			.min_rest_length = (body_a->get_radius() + body_b->get_radius()) * 1.02f, // 2% margin so rest state isn't sitting on the collision boundary
			.reduced_mass = reduced_mass };

		tendon->update_physics({ body_a->get_pos(), body_a->get_vel(), body_b->get_pos(), body_b->get_vel() },
			{ disable_length_breakage, disable_force_breakage }, stats);

		body_a->apply_force(tendon->movement_vector);
		body_b->apply_force(-tendon->movement_vector);
	}
}


void CellManager::check_for_extinction_event()
{
	// if protozoas are still alivee or if auto reset on extinction is disabled, we dont need to do anything
	if (all_cells_.size() >= extincion_threshold)
		return;

	// printing statistics about the simulation
	std::cout << "Extinction event occurred\n";

	extinction_event = true;
}


Cell* CellManager::find_cell_at_point(const sf::Vector2f mouse_position, bool make_selected_cell)
{
	// When this function is called it checks every cell in the world to see if the mouse click is within the bounds of any cell. 
	// If it finds a cell that contains the mouse position, it sets that cell as the selected cell and returns true. 
	//  bool make_selected_cell - If true, the cell that is found will be set as the selected cell.
	protozoa_tracker_.is_active = false;
	for (Cell* cell : all_cells_)
	{
		Body* body = bodies_->at(cell->body_id_);
		float dist_sq = (mouse_position - body->get_pos()).lengthSquared();
		if (dist_sq < body->get_radius() * body->get_radius())
		{
			// We tell the cell manager which cell is selected, 
			// so it can be used in other parts of the program (like rendering debug info for the selected cell).
			if (make_selected_cell)
			{
				selected_cell_id_ = cell->id_;
				protozoa_tracker_.is_active = true;
			}
			return cell;
		}
	}
	return nullptr;
}

void CellManager::fill_snapshot(SimSnapshot& snapshot, sf::FloatRect& visible_bounds) const
{
	snapshot.protozoa_tracker = protozoa_tracker_;

	snapshot.world_stats.highlighted_cells = selected_cells_indexes_.count;
}


const sf::Vector2f CellManager::get_selected_protozoa_pos() const
{
	// This function returns the position of the selected protozoa and 0, 0 if there is none
	if (selected_cell_id_ != -1)
	{
		Body* body = bodies_->at(all_cells_.at(selected_cell_id_)->body_id_);
		return body->get_pos();
	}
	return { 0.f, 0.f };
}

const sf::Vector2f CellManager::get_cell_pos(int cell_id)
{
	return bodies_->at(all_cells_.at(cell_id)->body_id_)->get_pos();
}

const void CellManager::set_cell_pos(int cell_id, const sf::Vector2f& position)
{
	bodies_->at(all_cells_.at(cell_id)->body_id_)->set_pos(position);
}

Body* CellManager::get_cell_body(int cell_id)
{
	return bodies_->at(all_cells_.at(cell_id)->body_id_);
}

void CellManager::drag_selected_cell_to_point(const sf::Vector2f& target_position, const float move_fraction)
{
	if (selected_cell_id_ == -1)
		return;

	Body* body = bodies_->at(all_cells_.at(selected_cell_id_)->body_id_);
	body->set_pos(target_position);

	const sf::Vector2f mouse_pos = m_window_->mapPixelToCoords(sf::Mouse::getPosition(*m_window_));
	const sf::Vector2f diff = mouse_pos - body->get_pos();
	body->set_pos(body->get_pos() + diff * move_fraction); // apply a small force towards the mouse position
}


CellBodyPair CellManager::create_cell(sf::Vector2f position, bool random_genetics)
{
	// This is the safest way to create a cell with a body, all creation events Must go through this function to ensure that the cell and body are linked correctly.
	// if there are not any already avalable cells in the o_vector we create a new one

	// Finding a body
	Body* body = bodies_->emplace(true, true);
	if (body == nullptr)
		return { .is_valid = false };

	// Finding a cell
	Cell* cell = all_cells_.emplace(true, true);
	if (cell == nullptr)
	{
		// raise an error as there shouldnt be a situation where we have a body but no cell, this should never happen
		std::cerr << "[ERROR]: Failed to create cell during initialization. Max cells reached.\n";
		bodies_->remove(body);
		return { .is_valid = false };
	}

	// resetting the cell just incase it isnt brand new
	cell->recreate();

	// resetting the body just incase it isnt brand new
	body->reset();

	// connecting the two
	cell->body_id_ = body->id_;
	body->set_pos(position);

	if (random_genetics)
	{
		cell->randomize();
	}

	body->mass_ = cell->mass;
	if (cell->mass == 0) body->mass_ = 0.1f;

	body->set_radius();

	register_birth_stat();

	return { cell->id_, body->id_, true };
}

bool CellManager::are_connected(cell_idx a, cell_idx b)
{
	const TendonLinks& links = all_cells_.at(a)->tendon_links_;
	for (uint32_t i = 0; i < links.size_; ++i)
	{
		const Tendon& s = *all_tendons_.at(links.connected_tendon_ids_[i]);
		if ((s.cell_A_id == a && s.cell_B_id == b) || (s.cell_A_id == b && s.cell_B_id == a))
			return true;
	}
	return false;
}

int32_t CellManager::create_tendon(const uint32_t cell_a_id, const uint32_t cell_b_id)
{
	if (are_connected(cell_a_id, cell_b_id)) return -1;

	// fetching the two cells
	Cell* cell_a = all_cells_.at(cell_a_id);
	Cell* cell_b = all_cells_.at(cell_b_id);

	// if either of the cells have reached their maximum number of connections, we cannot create a tendon between them
	if (cell_a->tendon_links_.is_full() || cell_b->tendon_links_.is_full())
		return -1;

	Tendon* tendon = all_tendons_.emplace(true, true);

	if (tendon == nullptr)
	{
		return -1;
	}
	tendon->reset();
	tendon->cell_A_id = cell_a_id;
	tendon->cell_B_id = cell_b_id;
	tendon->phase_ = cell_a->phase_; // a new tendon starts in step with its cells instead of fighting them

	cell_a->tendon_links_.add_connection(tendon->id_);
	cell_b->tendon_links_.add_connection(tendon->id_);

	return tendon->id_;
}
