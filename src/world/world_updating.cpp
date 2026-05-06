#include <algorithm>

#include "world.h"



void World::update()
{
	frame_rate_smoothing_.update_frame_rate();
	statistics_.updating_fps = frame_rate_smoothing_.get_average_frame_rate();
	toggles.min_speed += toggles.delta_min_speed;
	check_for_extinction_event(world_circular_bounds_);

	statistics_.iterations_++;
	update_position_container();

	if (toggles.track_statistics)
		update_statistics();

	resolve_collisions_threaded();  
	update_cell_collisions();

	food_manager_.update();
	resolve_food_interactions_threadded();

	update_all_cells();
}


void World::update_position_container()
{
	spatial_hash_grid_.clear();

	// Single pass: count cells
	statistics_.entity_count = all_cells_.size();

	// Resize all containers once, only when outside the buffer band
	const int container_size = static_cast<int>(collision_resolutions.size());
	if (statistics_.entity_count > container_size || statistics_.entity_count < container_size - 100)
	{
		const int new_size = statistics_.entity_count + 100;
		collision_resolutions.resize(new_size);
		render_data_.outer_colors.resize(new_size);
		render_data_.inner_colors.resize(new_size);
		render_data_.positions_x.resize(new_size);
		render_data_.positions_y.resize(new_size);
		render_data_.radii.resize(new_size);
		cell_pointers_.resize(new_size);
	}

	// Single pass: populate everything, zero collision data inline
	int idx = 0;
	for (Cell* cell : all_cells_)
	{
		cell->bound(world_circular_bounds_);

		render_data_.outer_colors[idx] = cell->get_outer_color();
		render_data_.inner_colors[idx] = cell->get_inner_color();
		render_data_.positions_x[idx] = cell->get_pos().x;
		render_data_.positions_y[idx] = cell->get_pos().y;
		render_data_.radii[idx] = cell->radius;

		cell_pointers_[idx] = cell;
		collision_resolutions[idx] = { 0.f, 0.f };  // zeroed inline

		spatial_hash_grid_.add_object(cell->get_pos().x, cell->get_pos().y, idx);
		++idx;
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

	float protozoa_count = static_cast<float>(all_cells_.size());

	if (protozoa_count == 0)
		return;

	statistics_.average_cells_per_protozoa = 0.f; // todo
	statistics_.average_offspring_count = 0.f;
	statistics_.average_mutation_rate = 0.f;
	statistics_.average_mutation_range = 0.f;

	// Calculating averages for cells per protozoa, offspring count, mutation rate, and mutation range across all protozoa
	int cell_count = 0;
	
	for (Cell* cell : all_cells_)
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
	if (statistics_.iterations_ % survival_rate_window_size_ == 0)
	{
		statistics_.deaths_per_hundered_frames = static_cast<float>(deaths_this_window_);
		statistics_.births_per_hundered_frames = static_cast<float>(births_this_window_);

		// Reset window
		deaths_this_window_ = 0;
		births_this_window_ = 0;
	}

	// Peak population
	const int p_count = static_cast<int>(all_cells_.size());
	statistics_.peak_protozoa_ever = std::max(p_count, statistics_.peak_protozoa_ever);

	// Per-organism aggregates
	float total_energy = 0.f;
	float total_springs = 0.f; // todo
	float sum_amp = 0.f, sum_sq = 0.f;
	int   cell_div_count = 0;

	for (Cell* c : all_cells_)
	{
		total_energy += c->energy;
		most_offspring_ever_ = std::max(c->offspring_count, most_offspring_ever_);
	
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

