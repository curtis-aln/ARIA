#include "../world.h"

void World::update_statistics()
{
	WorldStatistics& stats = statistics_;

	// World statistics
	stats.iterations_++;
	++stats.frames_since_last_gen_change;

	// count cells and food
	statistics_.cell_count = static_cast<int>(cell_manager_.get_cell_count());
	statistics_.food_count = static_cast<int>(food_manager_.get_food_vector().size());

	if (stats.cell_count == 0)
		return;

	cell_manager_.fill_statistics(stats);

	// Generation tracking - if the average generation has changed, we reset the frames_per_generation counter
	if (int(tracked_generation_) != int(stats.average_generation))
	{
		stats.frames_per_generation = (tracked_generation_ != 0.f)
			? (stats.iterations_ - stats.frames_since_last_gen_change) : stats.iterations_;
		stats.frames_since_last_gen_change = 0;
		tracked_generation_ = stats.average_generation;
	}
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