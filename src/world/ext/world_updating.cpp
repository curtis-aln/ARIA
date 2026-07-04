#include <algorithm>
#include "../world.h"


// This function is called by the simulation thread to update the world state
// write_snapshot is written to so the renderer knows what to draw
void World::update(SimSnapshot& write_snapshot)
{
	// Sanity Check
	if (get_entity_count() != bodies_.size())
		std::cerr << "Warning: Entity count mismatch! bodies_.size() = " << bodies_.size() << ", get_entity_count() = " << get_entity_count() << std::endl;
	
	if (should_drag_protozoa_)
		cell_manager_.drag_selected_cell_to_point(m_window_->mapPixelToCoords(sf::Mouse::getPosition(*m_window_)), 0.1f);

	if (toggles.m_tick_frame_time || !toggles.paused)
	{
		// updating the food and the cells in the world
		food_manager_.update();
		cell_manager_.update();

		update_entities();

		cell_manager_.update_food_interactions(food_manager_);
	}

	// We always update the position container, otherwise the simulation jitters when paused
	update_position_container(write_snapshot);

	// once the iteration has been completed, we update the statistics for the next frame
	if (toggles.track_statistics)
		update_statistics();

	fill_snapshot(write_snapshot);

	// This code allows up to step frame by frame through the update loop
	if (toggles.m_tick_frame_time) toggles.m_tick_frame_time = false;
}


void World::update_entities()
{
	// filling the containers that go to the renderer and to the spatial grid
	bound_bodies();

	collision_resolver_.add_particles_to_grid();
	collision_resolver_.run_collision_detection();
	collision_resolver_.handle_collision_resolutions();
}


void World::bound_bodies()
{
	for (Body* body : bodies_)
		bound_body_to_world(body);
}

void World::bound_body_to_world(Body* body)
{
	body->update_physics();

	constexpr float bounce_coefficient = 0.93f;

	// Circular boundary bounce
	const sf::Vector2f diff = body->position_ - world_circular_bounds_.center_;
	const float dist = diff.length();
	const float max_dist = world_circular_bounds_.bounds_radius - body->radius_;

	if (dist > max_dist && dist > 1e-6f)
	{
		const sf::Vector2f normal = diff / dist; // outward normal (center → body)

		// Only reflect if actually moving outward — avoids double-bounce
		// when collision resolution has already pushed the body inside
		const float vel_dot_n = body->velocity_.dot(normal);
		if (vel_dot_n > 0.f)
			body->velocity_ -= normal * ((1.f + bounce_coefficient) * vel_dot_n);

		// Clamp position flush to the inner surface
		body->position_ = world_circular_bounds_.center_ + normal * max_dist;
	}

	// very small attraction to the centre of the world
	body->velocity_ += (world_circular_bounds_.center_ - body->position_) * 0.0000001f;
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

void World::update_position_container(SimSnapshot& write_snapshot)
{
	/*
	if (statistics_.iterations_ % 2000 == 0)
		debug_sanity_checks();

	// Removing dead cells and food from the render data
	for (const Cell* cell : cell_manager_.get_all_cells())
	{
		const Body* body = bodies_.at(cell->body_id_);

		if (!bodies_.is_obj_active(cell->body_id_))
		{
			std::cout << "Removing MissMatched cell with body_id: " << cell->body_id_ << std::endl;
			bodies_.remove(cell->body_id_);
		}
	}
	*/

	// count cells and food
	statistics_.cell_count = static_cast<int>(cell_manager_.get_cell_count());
	statistics_.food_count = static_cast<int>(food_manager_.get_food_vector().size());

	RenderData& rend_data = write_snapshot.render;
	rend_data.outer_colors.resize(statistics_.cell_count);
	rend_data.inner_colors.resize(statistics_.cell_count);
	rend_data.positions.resize(statistics_.cell_count);
	rend_data.velocities.resize(statistics_.cell_count);
	rend_data.radii.resize(statistics_.cell_count);

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
