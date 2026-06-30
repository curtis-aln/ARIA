#include <algorithm>
#include "../world.h"


void World::update()
{
	frame_rate_smoothing_.update_frame_rate();
	toggles.min_speed += toggles.delta_min_speed;

	if (should_drag_protozoa_)
		cell_manager_.drag_selected_cell_to_point(m_window_->mapPixelToCoords(sf::Mouse::getPosition(*m_window_)), 0.1f);

	// filling the containers that go to the renderer and to the spatial grid
	update_position_container();

	// resolving collisions between all entities
	//if (statistics_.iterations_ % 3 == 0) // only recalculate the collision resolutions every 3 frames to save time
	calculate_collision_resolutions();
	//resolve_food_interactions_threadded();

	// applying the resolutions
	update_bodies();

	// updating the food and the cells in the world
	food_manager_.update();
	cell_manager_.update();

	cell_manager_.update_food_interactions(food_manager_);

	// once the iteration has been completed, we update the statistics for the next frame
	if (toggles.track_statistics)
		update_statistics();
}


void World::update_bodies()
{
	constexpr float inward_force = 0.0001f; // how strongly bodies are attracted to the centre of the sim

	int idx = 0;
	for (Body* body : bodies_)
	{
		body->position_ += collision_resolutions[idx]; // apply the collision resolution to the body's position
		body->velocity_ += velocity_resolutions[idx]; // apply the velocity resolution to the body's velocity

		const sf::Vector2f diff = world_circular_bounds_.center_ - body->position_;
		const sf::Vector2f normal = diff.normalized();
		body->velocity_ += normal * inward_force; // apply a small inward force to keep bodies from escaping the world bounds

		// physically clamp the body to the world bounds so that it doesn't escape
		body->position_ = world_circular_bounds_.contains(body->position_) ? body->position_ : world_circular_bounds_.center_ + normal * (world_circular_bounds_.bounds_radius - body->radius_);

		body->update_physics();
		idx++;
	}
}

void World::bound_body_to_world(Body* body)
{
	const sf::Vector2f center = world_circular_bounds_.center_;
	sf::Vector2f& position = body->position_;
	const float radius = body->radius_;
	const float world_radius = world_circular_bounds_.bounds_radius - radius;

	const sf::Vector2f direction = position - center;
	const float dist_sq = (position - center).lengthSquared();
	const float bounds_radius = world_radius - radius - radius * 0.1f; // Ensure the entire circle stays inside
	const float bounds_radius_sq = bounds_radius * bounds_radius;

	if (dist_sq > bounds_radius_sq) // Outside the boundary
	{
		const float dist = std::sqrt(dist_sq);
		sf::Vector2f normal = direction / dist; // Normalize direction

		// Move the circle back inside
		position = center + normal * bounds_radius;

		// Apply velocity adjustment to prevent escaping
		const float k = world_circular_bounds_.border_repulsion_magnitude;
		const float diff = dist - bounds_radius;
		body->accelerate(-normal * k * diff);
	}
}


void World::update_position_container()
{
	spatial_hash_grid_.clear();

	// Single pass: count cells and food
	statistics_.cell_count = static_cast<int>(cell_manager_.get_cell_count());
	statistics_.food_count = static_cast<int>(food_manager_.get_food_vector().size());
	statistics_.entity_count = statistics_.cell_count + statistics_.food_count;

	entity_positions_.resize(statistics_.entity_count);
	entity_radii_.resize(statistics_.entity_count);
	entity_velocities_.resize(statistics_.entity_count);
	velocity_resolutions.resize(statistics_.entity_count);
	collision_resolutions.resize(statistics_.entity_count);

	render_data_.outer_colors.resize(statistics_.entity_count);
	render_data_.inner_colors.resize(statistics_.entity_count);
	render_data_.positions_x.resize(statistics_.entity_count);
	render_data_.positions_y.resize(statistics_.entity_count);
	render_data_.radii.resize(statistics_.entity_count);

	// Resize all containers once, only when outside the buffer band
	//const int container_size = static_cast<int>(collision_resolutions.size());
	//if (statistics_.entity_count > container_size || statistics_.entity_count < container_size - 100)
	//{
	//	const int new_size = statistics_.entity_count + 100;
	//	collision_resolutions.resize(new_size);
	//	render_data_.outer_colors.resize(new_size);
	//	render_data_.inner_colors.resize(new_size);
	//	render_data_.positions_x.resize(new_size);
	//	render_data_.positions_y.resize(new_size);
	//	render_data_.radii.resize(new_size);
	//}

	// Single pass: populate everything, zero collision data inline
	int idx = 0;
	for (Body* body : bodies_)
	{
		// Fetching the cell's position from the body vector
		const sf::Vector2f& pos = body->position_;

		// Clamping the body into the world bounds so that the spatial grid doesn't get messed up by out-of-bounds positions
		bound_body_to_world(body);

		// Resetting collision resolution for this cell
		collision_resolutions[idx] = { 0.f, 0.f };
		velocity_resolutions[idx] = { 0.f, 0.f };

		entity_positions_[idx] = pos;

		entity_velocities_[idx] = body->velocity_;
		entity_radii_[idx] = body->radius_;

		spatial_hash_grid_.add_object(pos.x, pos.y, idx);
		++idx;
	}

	// updating render data
	int i = 0;
	for (const Cell* cell : cell_manager_.get_all_cells())
	{
		const Body* body = bodies_.at(cell->body_id_);

		render_data_.outer_colors[i] = cell->get_outer_color();
		render_data_.inner_colors[i] = cell->get_inner_color();
		render_data_.positions_x[i] = body->position_.x;
		render_data_.positions_y[i] = body->position_.y;
		render_data_.radii[i] = cell->radius;
		i++;
	}
}
