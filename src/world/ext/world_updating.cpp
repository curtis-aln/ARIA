#include <algorithm>
#include "../world.h"


void World::update()
{
	if (get_entity_count() != bodies_.size())
	{
		std::cerr << "Warning: Entity count mismatch! bodies_.size() = " << bodies_.size() << ", get_entity_count() = " << get_entity_count() << std::endl;
	}

	frame_rate_smoothing_.update_frame_rate();
	toggles.min_speed += toggles.delta_min_speed;

	if (should_drag_protozoa_)
		cell_manager_.drag_selected_cell_to_point(m_window_->mapPixelToCoords(sf::Mouse::getPosition(*m_window_)), 0.1f);

	update_entities();

	// updating the food and the cells in the world
	food_manager_.update();
	cell_manager_.update();

	cell_manager_.update_food_interactions(food_manager_);

	// once the iteration has been completed, we update the statistics for the next frame
	if (toggles.track_statistics)
		update_statistics();
}


void World::update_entities()
{
	collision_resolver_.add_particles_to_grid();
	collision_resolver_.run_collision_detection();
	collision_resolver_.handle_collision_resolutions();

	// filling the containers that go to the renderer and to the spatial grid
	bound_bodies();

	update_position_container();
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
}


void World::update_position_container()
{
	// Single pass: count cells and food
	statistics_.cell_count = static_cast<int>(cell_manager_.get_cell_count());
	statistics_.food_count = static_cast<int>(food_manager_.get_food_vector().size());
	statistics_.entity_count = statistics_.cell_count + statistics_.food_count;

	render_data_.outer_colors.resize(statistics_.cell_count);
	render_data_.inner_colors.resize(statistics_.cell_count);
	render_data_.positions.resize(statistics_.cell_count);
	render_data_.velocities.resize(statistics_.cell_count);
	render_data_.radii.resize(statistics_.cell_count);

	// updating render data
	int i = 0;
	for (const Cell* cell : cell_manager_.get_all_cells())
	{
		const Body* body = bodies_.at(cell->body_id_);

		if (!bodies_.is_obj_active(cell->body_id_))
			continue;

		render_data_.outer_colors[i] = cell->get_outer_color();
		render_data_.inner_colors[i] = cell->get_inner_color();
		render_data_.positions[i] = body->position_;
		render_data_.velocities[i] = body->velocity_;
		render_data_.radii[i] = cell->radius;
		i++;
	}

	render_data_.outer_colors.resize(i);
	render_data_.inner_colors.resize(i);
	render_data_.positions.resize(i);
	render_data_.radii.resize(i);
	render_data_.velocities.resize(i);
}
