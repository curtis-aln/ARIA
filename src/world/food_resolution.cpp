#include "world.h"


void World::init_food_jobs()
{
	const int total = spatial_hash_grid_.CellsX * spatial_hash_grid_.CellsY;
	const int chunk = std::max(1, (total + updating_threads - 1) / updating_threads);

	food_jobs_.reserve(updating_threads);

	for (int t = 0; t < updating_threads; ++t)
	{
		const int begin = t * chunk;
		if (begin >= total) break;
		const int end = std::min(begin + chunk, total);

		food_jobs_.push_back([this, begin, end]
			{
				for (int cell_id = begin; cell_id < end; ++cell_id)
					resolve_food_grid_cell(cell_id, tl_nearby_food);
			});
	}

	food_thread_pool_.assign_jobs(food_jobs_);
}


void World::resolve_food_interactions()
{
	for (int cell_id = 0; cell_id < spatial_hash_grid_.CellsX * spatial_hash_grid_.CellsY; ++cell_id)
	{
		resolve_food_grid_cell(cell_id, tl_nearby_food);
	}
}

void World::resolve_food_interactions_threadded()
{
	claim_buffer.reset(food_manager_.get_size());
	collision_thread_pool_.run_and_wait();  // no allocation, just resubmit

	for (int i = claim_buffer.active_count() - 1; i >= 0; --i)
		if (claim_buffer.is_claimed(i))
			food_manager_.remove_food(i);
}


void World::resolve_food_grid_cell(const int cell_id, FixedSpan<obj_idx>& nearby_food)
{
	SimpleSpatialGrid* food_grid = get_food_spatial_grid();

	// if the cell grid cell is empty, dont bother
	if (spatial_hash_grid_.cell_capacities[cell_id] == 0)
		return;

	food_grid->find_from_index(cell_id, &nearby_food);

	// now we get all the cells in the same cell grid
	const obj_idx* cell_contents = &spatial_hash_grid_.grid[cell_id * spatial_hash_grid_.cell_max_capacity];
	const int cell_size = spatial_hash_grid_.cell_capacities[cell_id];

	for (int idx = 0; idx < cell_size; ++idx)
	{
		// After
		
		Cell* cell = cell_pointers_[cell_contents[idx]];

		for (int i = 0; i < nearby_food.count; ++i)
		{
			Food* food = food_manager_.at(nearby_food[i]);
			// Check proximity BEFORE claiming
			if (!Cell::consume_food_check(*cell, food))
				continue;  // too far, try next food

			// Close enough — now race to claim it
			if (!claim_buffer.claim(nearby_food[i]))
				continue;  // another thread already ate it

			cell->eat(food->nutrients);
			return;

		}
	}
}
