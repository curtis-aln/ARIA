#include <algorithm>

#include "../world.h"



void World::update()
{
	frame_rate_smoothing_.update_frame_rate();
	statistics_.updating_fps = frame_rate_smoothing_.get_average_frame_rate();
	toggles.min_speed += toggles.delta_min_speed;

	statistics_.iterations_++;
	update_position_container();

	if (toggles.track_statistics)
		update_statistics();

	
	resolve_collisions_threaded();

	resolve_food_interactions_threadded();

	update_bodies();

	food_manager_.update();

	cell_manager_.update();
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
	statistics_.protozoa_count = static_cast<int>(cell_manager_.all_cells_.size());
	statistics_.food_count = static_cast<int>(food_manager_.get_food_vector().size());
	statistics_.entity_count = statistics_.protozoa_count + statistics_.food_count;

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
	for (Cell* cell : cell_manager_.all_cells_)
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


void World::update_statistics()
{
	++statistics_.frames_since_last_gen_change;

	statistics_.average_generation = get_average_generation();

	if (int(tracked_generation_) != int(statistics_.average_generation))
	{
		statistics_.frames_per_generation = (tracked_generation_ != 0.f)
			? (statistics_.iterations_ - statistics_.frames_since_last_gen_change) : statistics_.iterations_;
		statistics_.frames_since_last_gen_change = 0;
		tracked_generation_ = statistics_.average_generation;
	}

	float protozoa_count = static_cast<float>(cell_manager_.all_cells_.size());

	if (protozoa_count == 0)
		return;

	statistics_.average_cells_per_protozoa = 0.f; // todo
	statistics_.average_offspring_count = 0.f;
	statistics_.average_mutation_rate = 0.f;
	statistics_.average_mutation_range = 0.f;

	// Calculating averages for cells per protozoa, offspring count, mutation rate, and mutation range across all protozoa
	int cell_count = 0;
	
	for (Cell* cell : cell_manager_.all_cells_)
	{
		statistics_.average_mutation_rate += cell->mutation_rate;
		statistics_.average_mutation_range += cell->mutation_range;
		statistics_.average_offspring_count += cell->offspring_count;

		cell_count++;
	}


	statistics_.average_cells_per_protozoa /= protozoa_count;
	statistics_.average_offspring_count /= protozoa_count;
	statistics_.average_mutation_rate /= cell_count;
	statistics_.average_mutation_range /= cell_count;

	// Updating death and birth rates per hundred frames
	if (statistics_.iterations_ % cell_manager_.survival_rate_window_size_ == 0)
	{
		statistics_.deaths_per_hundered_frames = static_cast<float>(cell_manager_.deaths_this_window_);
		statistics_.births_per_hundered_frames = static_cast<float>(cell_manager_.births_this_window_);

		// Reset window
		cell_manager_.deaths_this_window_ = 0;
		cell_manager_.births_this_window_ = 0;
	}

	// Peak population
	const int p_count = static_cast<int>(cell_manager_.all_cells_.size());
	statistics_.peak_protozoa_ever = std::max(p_count, statistics_.peak_protozoa_ever);

	// Per-organism aggregates
	float total_energy = 0.f;
	float total_springs = 0.f; // todo
	float sum_amp = 0.f, sum_sq = 0.f;
	int   cell_div_count = 0;

	for (Cell* c : cell_manager_.all_cells_)
	{
		total_energy += c->energy;
		cell_manager_.most_offspring_ever_ = std::max(c->offspring_count, cell_manager_.most_offspring_ever_);
	
		//total_springs += static_cast<float>(c->get_springs().size());

		sum_amp += c->amplitude;
		sum_sq += c->amplitude * c->amplitude;
		++cell_div_count;
		statistics_.highest_generation_ever = std::max(c->generation, statistics_.highest_generation_ever);

	}

	if (protozoa_count > 0)
	{
		statistics_.average_energy = total_energy / protozoa_count;
		statistics_.average_spring_count = total_springs / protozoa_count;
		statistics_.energy_efficiency = statistics_.average_energy / 300.f; // ratio vs starting energy
	}
	if (cell_div_count > 0)
	{
		const float mean_amp = sum_amp / cell_div_count;
		statistics_.genetic_diversity = (sum_sq / cell_div_count) - (mean_amp * mean_amp);
	}
}

