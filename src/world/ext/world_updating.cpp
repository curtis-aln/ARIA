#include <algorithm>
#include "../world.h"


// This function is called by the simulation thread to update the world state
// write_snapshot is written to so the renderer knows what to draw
void World::update(SimSnapshot& write_snapshot)
{
	visible_bounds = calulcate_visible_range();

	// Sanity Check
	if (get_entity_count() != bodies_.size())
		std::cerr << "Warning: Entity count mismatch! bodies_.size() = " << bodies_.size() << ", get_entity_count() = " << get_entity_count() << std::endl;
	
	if (should_drag_protozoa_)
		cell_manager_.drag_selected_cell_to_point(m_window_->mapPixelToCoords(sf::Mouse::getPosition(*m_window_)), 0.1f);

	if (toggles.m_tick_frame_time || !toggles.paused)
	{
		// updating the food and the cells in the world
		food_manager_.update(write_snapshot.food_data);
		cell_manager_.update();

		update_entities();

		if (statistics_.iterations_ % 30 == 0)
			food_eat_resolver_.resolve();
		else
			food_eat_resolver_.resolve_existing_detections();
	}

	// We always update the position container, otherwise the simulation jitters when paused
	update_position_container_optimized(write_snapshot);

	// once the iteration has been completed, we update the statistics for the next frame
	if (toggles.track_statistics)
		update_statistics();

	fill_snapshot(write_snapshot);

	// This code allows up to step frame by frame through the update loop
	if (toggles.m_tick_frame_time) toggles.m_tick_frame_time = false;
}

void World::ensure_update_jobs_built()
{
	if (update_jobs_built_)
		return;

	updating_bodies_.clear();
	updating_bodies_.reserve(updating_threads);

	// For each of the threads
	for (int t = 0; t < (int)updating_threads; ++t)
	{
		updating_bodies_.emplace_back([this, t] {
			const int total_cells = current_total_bodies_;
			if (total_cells == 0)
				return;

			const int chunk = std::max(1, (total_cells + (int)updating_threads - 1) / (int)updating_threads);
			const int begin = t * chunk;
			if (begin >= total_cells)
				return;
			const int end = std::min(begin + chunk, total_cells);

			for (int k = begin; k < end; ++k)
			{
				Body* body = bodies_.at(bodies_.occupied_list[k]);
				bound_body_to_world(body);

			}});
	}

	thread_pool_.set_jobs(updating_bodies_);   // only ever called this once
	update_jobs_built_ = true;
}


void World::update_entities()
{
	// filling the containers that go to the renderer and to the spatial grid
	bound_bodies();

	if (!toggles.toggle_collisions)
		return;
	
	if (statistics_.iterations_ % 30 != 0)
	{
		collision_resolver_.resolve_existing_detections();
		collision_resolver_.handle_collision_resolutions();
	}
	else
	{
		collision_resolver_.add_particles_to_grid();
		collision_resolver_.run_collision_detection();
		collision_resolver_.handle_collision_resolutions();
	}
}


void World::bound_bodies()
{
	current_total_bodies_ = bodies_.occupied_count;
	ensure_update_jobs_built();
	thread_pool_.run_and_wait();
}

void World::bound_body_to_world(Body* body)
{
	body->update_physics();

	constexpr float bounce_coefficient = 0.93f;

	// Circular boundary bounce
	const sf::Vector2f diff = body->position_ - world_circular_bounds_.center_;
	const float dist_sq = diff.x * diff.x + diff.y * diff.y;

	const float max_dist = world_circular_bounds_.bounds_radius - (body->radius_ * 2.f);
	const float max_dist_sq = max_dist * max_dist;

	if (dist_sq > max_dist_sq && dist_sq > 0.0001f)
	{
		// Reflect velocity using the raw (unnormalized) diff instead of a unit normal.
		// v -= n * (2*v.n) with n = diff/sqrt(dist_sq) simplifies algebraically to:
		// v -= diff * (2 * v.diff / dist_sq)  -- same result, zero sqrt calls.
		const float vel_dot_diff = body->velocity_.dot(diff);
		if (vel_dot_diff > 0.f)
			body->velocity_ -= diff * ((1.f + bounce_coefficient) * vel_dot_diff / dist_sq);

		// Soft position correction: nudge back proportional to overshoot instead of
		// clamping exactly onto the circle (that would need sqrt too). Converges
		// quickly and is visually indistinguishable from a hard clamp for small overshoots.
		const float correction = (dist_sq - max_dist_sq) / dist_sq;
		body->position_ -= diff * correction;
	}

	// very small attraction to the centre of the world
	body->velocity_ += (world_circular_bounds_.center_ - body->position_) * 0.000000015f;
}


void World::debug_sanity_checks()
{
	for (const Cell* cell : cell_manager_.get_all_cells())
	{
		const Body* body = bodies_.at(cell->body_id_);

		if (!bodies_.is_obj_active(cell->body_id_))
		{
			std::cout << "Removing MissMatched cell with body_id: " << cell->body_id_ << std::endl;
			bodies_.remove(cell->body_id_);
		}
	}

	for (const Food* cell : food_manager_.get_food_vector())
	{
		const Body* body = bodies_.at(cell->body_id_);

		if (!bodies_.is_obj_active(cell->body_id_))
		{
			std::cout << "Removing MissMatched food with body_id: " << cell->body_id_ << std::endl;
			bodies_.remove(cell->body_id_);
		}
	}

	for (Body* body : bodies_)
	{
		// Finding out if the body is missing a cell or food
		if (!cell_manager_.has_cell_with_body_id(body->id_) && !food_manager_.has_food_with_body_id(body->id_))
		{
			std::cout << "MissMatched body with id: " << body->id_ << std::endl;
			//bodies_.remove(body->id_);
		}
	}
}

sf::FloatRect World::calulcate_visible_range()
{
	auto window_size = static_cast<sf::Vector2i>(m_window_->getSize());
	sf::Vector2f top_left = m_window_->mapPixelToCoords({ 0, 0 });
	sf::Vector2f bottom_right = m_window_->mapPixelToCoords(sf::Vector2i{ window_size.x, window_size.y });

	sf::FloatRect visible_bounds = {
		{top_left.x, top_left.y},
		{bottom_right.x - top_left.x, bottom_right.y - top_left.y} };

	// Adjust for cell maximim radius to ensure cells partially in view are included
	const float max_rad = CellGeneticConstraints::radius.max;
	visible_bounds.position.x -= max_rad;
	visible_bounds.position.y -= max_rad;
	visible_bounds.size.x += max_rad * 2.f;
	visible_bounds.size.y += max_rad * 2.f;

	return visible_bounds;
}

void World::update_position_container_optimized(SimSnapshot& write_snapshot)
{
	RenderData& rend_data = write_snapshot.render;
	rend_data.outer_colors.clear();
	rend_data.inner_colors.clear();
	rend_data.positions.clear();
	rend_data.velocities.clear();
	rend_data.radii.clear();

	for (const Cell* cell : cell_manager_.get_all_cells())
	{
		Body* body = bodies_.at(cell->body_id_);
		if (!visible_bounds.contains(body->position_))
			continue;

		sf::Color color_inner = !cell->is_alive() ? sf::Color(60, 60, 60, 180) : cell->get_inner_color();
		sf::Color color_outer = !cell->is_alive() ? sf::Color(90, 90, 90, 160) : cell->get_outer_color();
		
		rend_data.outer_colors.push_back(color_outer);
		rend_data.inner_colors.push_back(color_inner);
		rend_data.positions.push_back(body->position_);
		rend_data.velocities.push_back(body->velocity_);
		rend_data.radii.push_back(cell->radius);
	}
}

void World::update_position_container(SimSnapshot& write_snapshot)
{
	RenderData& rend_data = write_snapshot.render;
	int n = cell_manager_.get_cell_count();
	rend_data.outer_colors.resize(n);
	rend_data.inner_colors.resize(n);
	rend_data.positions.resize(n);
	rend_data.velocities.resize(n);
	rend_data.radii.resize(n);

	// updating render data
	int i = 0;
	for (const Cell* cell : cell_manager_.get_all_cells())
	{
		const Body* body = bodies_.at(cell->body_id_);

		rend_data.outer_colors[i] = cell->get_outer_color();
		rend_data.inner_colors[i] = cell->get_inner_color();
		rend_data.positions[i] = body->position_;
		rend_data.velocities[i] = body->velocity_;
		rend_data.radii[i] = cell->radius;
		i++;
	}

}
