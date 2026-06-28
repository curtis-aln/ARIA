#include "../world.h"

void World::update_statistics()
{
	WorldStatistics& stats = statistics_;

	// World statistics
	stats.iterations_++;
	stats.updating_fps = frame_rate_smoothing_.get_average_frame_rate();
	++stats.frames_since_last_gen_change;

	// Basic Fetching Statistics
	stats.cell_count = cell_manager_.get_cell_count();
	stats.food_count = food_manager_.get_size();
	stats.entity_count = stats.cell_count + stats.food_count;

	// Cell Manager statistics
	stats.average_generation = cell_manager_.calculate_average_generation();
	stats.average_lifetime = cell_manager_.average_lifetime_;
	stats.longest_lived_ever = cell_manager_.longest_lived_ever_;

	// Generation tracking - if the average generation has changed, we reset the frames_per_generation counter
	if (int(tracked_generation_) != int(stats.average_generation))
	{
		stats.frames_per_generation = (tracked_generation_ != 0.f)
			? (stats.iterations_ - stats.frames_since_last_gen_change) : stats.iterations_;
		stats.frames_since_last_gen_change = 0;
		tracked_generation_ = stats.average_generation;
	}

	if (stats.cell_count == 0)
		return;

	calculate_averaging_statistics();

	// Updating death and birth rates per hundred frames
	if (stats.iterations_ % cell_manager_.survival_rate_window_size_ == 0)
	{
		stats.deaths_per_hundered_frames = static_cast<float>(cell_manager_.deaths_this_window_);
		stats.births_per_hundered_frames = static_cast<float>(cell_manager_.births_this_window_);

		// Reset window
		cell_manager_.deaths_this_window_ = 0;
		cell_manager_.births_this_window_ = 0;
	}

	// Peak population
	const int p_count = static_cast<int>(cell_manager_.all_cells_.size());
	stats.peak_protozoa_ever = std::max(p_count, stats.peak_protozoa_ever);
}


void World::calculate_averaging_statistics()
{
	WorldStatistics& stats = statistics_;
	const int count = stats.cell_count;

	// resetting averages
	stats.average_offspring_count = 0.f;
	stats.average_mutation_rate = 0.f;
	stats.average_mutation_range = 0.f;
	stats.average_energy = 0.f;

	// collecting data
	for (Cell* cell : cell_manager_.all_cells_)
	{
		stats.average_mutation_rate += cell->mutation_rate;
		stats.average_mutation_range += cell->mutation_range;
		stats.average_offspring_count += cell->offspring_count;
		stats.average_energy += cell->energy;

		stats.highest_generation_ever = std::max(cell->generation, stats.highest_generation_ever);
		stats.most_offspring_ever = std::max(static_cast<int>(cell->offspring_count), stats.most_offspring_ever);
	}

	// calculating averages
	stats.average_offspring_count /= count;
	stats.average_mutation_rate /= count;
	stats.average_mutation_range /= count;
	stats.average_energy /= count;
}

SpatialGridData World::get_grid_data(SimpleSpatialGrid* grid)
{
	return SpatialGridData{
		grid->CellsX,
		grid->CellsY,
		grid->cell_max_capacity,
		grid->world_width,
		grid->world_height,
		grid->cell_width,
		grid->cell_height,
	};
}

void World::calculate_spatial_grid_statistics(SimpleSpatialGrid* grid, SpatialGridData& data)
{
	data.total = 0;
	data.max_in = 0;
	data.full = 0;
	data.empty = 0;
	for (size_t i = 0; i < grid->get_total_cells(); ++i)
	{
		const uint8_t cell_capacity = grid->cell_capacities[i];
		data.total += cell_capacity;
		data.max_in = std::max<int>(cell_capacity, data.max_in);
		if (cell_capacity == grid->cell_max_capacity)
			data.full++;
		if (cell_capacity == 0)
			data.empty++;
	}
	data.inv = data.total > 0 ? 1.f / static_cast<float>(data.total) : 0.f;
}