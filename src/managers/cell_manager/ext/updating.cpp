#include "../cell_manager.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <entities/body.h>
#include <entities/cell/cell.h>
#include <entities/matter/CellMatter.h>
#include <entities/spring/spring.h>
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
		bool is_newborn = cell->internal_clock_ < infant_time;
		bool is_first_gen = cell->generation == 0; // we dont want the initial cells during the sim start to all tangle
		if (!is_newborn || is_first_gen)
			continue;

		Body* body = bodies_->at(cell->body_id_);
		new_born_cell_grid_.add_object(body->position_.x, body->position_.y, cell->id_);
	}
}


void CellManager::update(int iterations)
{
	if (extinction_event)
		return;

	// this has to happen before we move about the cells
	if (!toggles_.easy_reproduction) // if easy reproduction is on, we dont want to add new cells to the grid because they will be connected immediately
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
	collect_spring_death_requests();

	apply_cell_death_requests();
	apply_matter_death_requests();
	apply_spring_death_requests();

	// --------------- updating cells and matter ---------------
	spawn_immune = iterations < init_spring_immunity_time;

	update_springs(spawn_immune);
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
	body->velocity_ *= cell->sinwave_current_friction_;

	speed_tax_cell(cell);
	impulse_tax_cell(cell, body->impulse_);
}

void CellManager::impulse_tax_cell(Cell* cell, const float impulse) const
{
	if (impulse < impulse_damage_thresh || spawn_immune || !toggles_.collision_integrity_damage_)
		return;

	float damage = -(impulse - impulse_damage_thresh) * impulse_damage_multiplier;
	damage = std::max(damage, -cell->get_integrity() * max_single_hit_integrity_fraction);

	cell->change_integrity(damage);
	cell->cumulative_collision_damage_ += std::abs(damage);
}

void CellManager::collect_connection_requests()
{
	for (Cell* cell : all_cells_)
	{
		bool is_newborn = cell->internal_clock_ < infant_time;
		bool is_check_frame = cell->internal_clock_ % infant_check_interval == 0;
		bool is_first_gen = cell->generation == 0;
		bool can_have_more_connections = cell->new_connections_made < cell->max_cell_connections;

		if (is_newborn && is_check_frame && !is_first_gen && can_have_more_connections)
			try_connect_newborn_cell(cell);
	}
}


void CellManager::update_position_container(RenderData& rend_data, const sf::FloatRect& visible_bounds, const bool show_only_newborns)
{
	size_t current_vector_size = rend_data.positions.size();
	unsigned cell_count = get_cell_count();

	for (const Cell* cell : all_cells_)
	{
		Body* body = bodies_->at(cell->body_id_);
		if (!visible_bounds.contains(body->position_))
			continue;

		if (show_only_newborns && cell->internal_clock_ >= infant_time)
			continue;

		rend_data.outer_colors.push_back(cell->get_outer_color());
		rend_data.inner_colors.push_back(cell->get_inner_color());
		rend_data.positions.push_back(body->position_);
		rend_data.velocities.push_back(body->velocity_);
		rend_data.radii.push_back(cell->radius);
	}

	const int n = get_cell_count();

	// now we handle springs, we can just store the indexes as then the renderer can read them from the positions container above
	const int spring_count = static_cast<int>(all_springs_.size());
	rend_data.spring_connections.clear();

	for (Spring* spring : all_springs_)
	{
		Cell* cell_a = all_cells_.at(spring->cell_A_id);
		Body* body_a = bodies_->at(cell_a->body_id_);
		bool cell_a_visible = visible_bounds.contains(body_a->position_);
		bool cell_a_newborn = cell_a->internal_clock_ < infant_time;

		if (show_only_newborns && cell_a_newborn)
			continue;

		Cell* cell_b = all_cells_.at(spring->cell_B_id);
		Body* body_b = bodies_->at(cell_b->body_id_);

		bool cell_b_visible = visible_bounds.contains(body_b->position_);
		bool cell_b_newborn = cell_b->internal_clock_ < infant_time;

		if (show_only_newborns && cell_b_newborn)
			continue;

		if (!cell_a_visible && !cell_b_visible)
			continue;

		const float min_dist = body_a->radius_ + body_b->radius_;

		// The reason why we cant use indexing to fill this array is because we dont know how many bodies are not active,
		// so it messes with the indexing and leads to null connections
		rend_data.spring_connections.push_back({
			get_cell_pos(spring->cell_A_id),
			get_cell_pos(spring->cell_B_id),
			spring->genome.outer_r, spring->genome.outer_g, spring->genome.outer_b,
			min_dist,
			min_dist * 3.f,
			spring->stress });
	}

	if (show_only_newborns)
		return;

	for (const CellMatter* matter : all_cell_matter_)
	{
		Body* body = bodies_->at(matter->body_id_);

		if (!visible_bounds.contains(body->position_))
			continue;

		rend_data.outer_colors.push_back(matter->outer_color());
		rend_data.inner_colors.push_back(matter->inner_color());
		rend_data.positions.push_back(body->position_);
		rend_data.velocities.push_back(body->velocity_);
		rend_data.radii.push_back(body->radius_);
	}
}

// updating.cpp
void CellManager::try_connect_newborn_cell(Cell* cell)
{
	Body* body = bodies_->at(cell->body_id_);

	nearby_cell_ids.clear();
	new_born_cell_grid_.find(body->position_.x, body->position_.y, &nearby_cell_ids);

	for (int i = 0; i < nearby_cell_ids.count; ++i)
	{
		cell_idx other_cell_id = nearby_cell_ids[i];
		if (other_cell_id == cell->id_)
			continue;

		// Canonical ordering: only the lower-id cell in a pair ever issues
		// the request. Both cells sit in the same grid and would otherwise
		// discover each other independently -> two requests, two springs.
		if (cell->id_ > other_cell_id)
			continue;

		Cell* other_cell = all_cells_.at(other_cell_id);

		if (cell->already_connected_to(other_cell_id))
			continue;

		sf::Vector2f dir = body->position_ - bodies_->at(other_cell->body_id_)->position_;
		float dist_sq = dir.lengthSquared();
		if (dist_sq < cell->newborn_search_radius * cell->newborn_search_radius)
		{
			connection_requests.push_back(ConnectionRequest{ (int32_t)cell->id_, (int32_t)other_cell->id_ });
			cell->register_connection(other_cell_id);
			other_cell->register_connection(cell->id_);
		}
	}
}


void CellManager::update_springs(bool immune)
{
	for (Spring* spring : all_springs_)
	{
		Cell* cell_a = all_cells_.at(spring->cell_A_id);
		Cell* cell_b = all_cells_.at(spring->cell_B_id);

		bool true_immune = immune || !toggles_.spring_stress_integrity_damage;
		spring->update_organics(*cell_a, *cell_b, !true_immune, toggles_.work_done_energy);

		// otherwise we update the spring physics and organics
		Body* body_a = bodies_->at(cell_a->body_id_);
		Body* body_b = bodies_->at(cell_b->body_id_);

		bool disable_length_breakage = immune || !toggles_.spring_too_long_breakage;
		bool disable_force_breakage = immune || !toggles_.spring_too_much_force_breakage;
		spring->update_physics(body_a->position_, body_a->velocity_, body_b->position_, body_b->velocity_, disable_length_breakage, disable_force_breakage);
		body_a->accelerate(spring->movement_vector);
		body_b->accelerate(-spring->movement_vector);
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
		float dist_sq = (mouse_position - body->position_).lengthSquared();
		if (dist_sq < cell->radius * cell->radius)
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


const sf::Vector2f* CellManager::get_selected_protozoa_pos() const
{
	// This function returns the position of the selected protozoa, 
	// if there is one.
	if (selected_cell_id_ != -1)
	{
		Body* body = bodies_->at(all_cells_.at(selected_cell_id_)->body_id_);
		return &body->position_;
	}
	return nullptr;
}

sf::Vector2f& CellManager::get_cell_pos(int cell_id)
{
	return bodies_->at(all_cells_.at(cell_id)->body_id_)->position_;
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
	body->position_ = target_position;

	const sf::Vector2f mouse_pos = m_window_->mapPixelToCoords(sf::Mouse::getPosition(*m_window_));
	const sf::Vector2f diff = mouse_pos - body->position_;
	body->position_ += diff * move_fraction; // apply a small force towards the mouse position
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
	body->reset_cell_manager();

	// connecting the two
	cell->body_id_ = body->id_;
	body->position_ = position;

	if (random_genetics)
	{
		cell->randomize();
	}

	body->radius_ = cell->radius;
	body->mass_ = body->radius_;

	register_birth_stat();

	return { cell->id_, body->id_, true };
}

int32_t CellManager::create_spring(const uint32_t cell_a_id, const uint32_t cell_b_id)
{
	// fetching the two cells
	Cell* cell_a = all_cells_.at(cell_a_id);
	Cell* cell_b = all_cells_.at(cell_b_id);

	// if either of the cells have reached their maximum number of connections, we cannot create a spring between them
	if (cell_a->spring_links_.is_full() || cell_b->spring_links_.is_full())
		return -1;

	Spring* spring = all_springs_.emplace(true, true);

	if (spring == nullptr)
	{
		return -1;
	}
	spring->reset();
	spring->cell_A_id = cell_a_id;
	spring->cell_B_id = cell_b_id;

	cell_a->spring_links_.add_connection(spring->id_);
	cell_b->spring_links_.add_connection(spring->id_);

	return spring->id_;
}
