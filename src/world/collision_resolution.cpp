#include "world.h"

void World::init_collision_jobs()
{
	const int total = spatial_hash_grid_.CellsX * spatial_hash_grid_.CellsY;
	const int chunk = std::max(1, (total + updating_threads - 1) / updating_threads);

	collision_jobs_.clear();
	collision_jobs_.reserve(updating_threads);

	for (int t = 0; t < updating_threads; ++t)
	{
		const int begin = t * chunk;
		if (begin >= total) break;
		const int end = std::min(begin + chunk, total);

		collision_jobs_.push_back([this, begin, end]
			{
				for (int cell_id = begin; cell_id < end; ++cell_id)
					update_cells_in_grid_cell(cell_id, tl_nearby_ids);
			});
	}

	collision_thread_pool_.assign_jobs(collision_jobs_);
}

void World::resolve_collisions_threaded()
{
	if (!toggles.toggle_collisions)
		return;

	collision_thread_pool_.run_and_wait();
}

void World::resolve_collisions()
{
	if (!toggles.toggle_collisions)
		return;

	
	for (int cell_id = 0; cell_id < spatial_hash_grid_.CellsX * spatial_hash_grid_.CellsY; ++cell_id)
	{
		update_cells_in_grid_cell(cell_id, tl_nearby_ids);
	}
}

void World::update_cells_in_grid_cell(const int grid_cell_id, FixedSpan<uint32_t>& nearby_ids)
{
	// if the grid cell is empty, dont bother
	if (spatial_hash_grid_.cell_capacities[grid_cell_id] == 0)
		return;

	nearby_ids.clear();
	int neighbours_size = 0;

	const int cell_index_x = grid_cell_id % spatial_hash_grid_.CellsX;
	const int cell_index_y = grid_cell_id / spatial_hash_grid_.CellsX;

	// Current cell
	update_nearby_container(cell_index_x, cell_index_y, nearby_ids);

	// Right
	update_nearby_container(cell_index_x + 1, cell_index_y, nearby_ids);

	// Bottom-left
	update_nearby_container(cell_index_x - 1, cell_index_y + 1, nearby_ids);

	// Bottom
	update_nearby_container(cell_index_x, cell_index_y + 1, nearby_ids);

	// Bottom-right
	update_nearby_container(cell_index_x + 1, cell_index_y + 1, nearby_ids);

	// Left
	update_nearby_container(cell_index_x - 1, cell_index_y, nearby_ids);

	// Top-left
	update_nearby_container(cell_index_x - 1, cell_index_y - 1, nearby_ids);

	// Top
	update_nearby_container(cell_index_x, cell_index_y - 1, nearby_ids);

	// Top-right
	update_nearby_container(cell_index_x + 1, cell_index_y - 1, nearby_ids);


	const auto& cell_contents = spatial_hash_grid_.grid[grid_cell_id];
	const uint8_t cell_size = spatial_hash_grid_.cell_capacities[grid_cell_id];

	for (uint8_t idx = 0; idx < cell_size; ++idx)
	{
		update_protozoa_cell(cell_contents[idx], nearby_ids);
	}
}


void World::update_nearby_container(int32_t neighbour_index_x, int32_t neighbour_index_y, FixedSpan<uint32_t>& nearby_ids)
{
	// Out of bounds check, no wrapping needed
	if (neighbour_index_x < 0 || neighbour_index_x >= static_cast<int>(spatial_hash_grid_.CellsX) ||
		neighbour_index_y < 0 || neighbour_index_y >= static_cast<int>(spatial_hash_grid_.CellsY))
		return;

	const uint32_t neighbour_index = neighbour_index_y * spatial_hash_grid_.CellsX + neighbour_index_x;
	const auto& contents = spatial_hash_grid_.grid[neighbour_index];
	const uint8_t size = spatial_hash_grid_.cell_capacities[neighbour_index];


	for (uint8_t idx = 0; idx < size; ++idx)
	{
		nearby_ids.add(contents[idx]);
	}
}

void World::update_protozoa_cell(const int protozoa_cell_index, const FixedSpan<uint32_t>& nearby_ids)
{
	const float pos_a_x = render_data_.positions_x[protozoa_cell_index];
	const float pos_a_y = render_data_.positions_y[protozoa_cell_index];

	for (const uint32_t id : nearby_ids)
	{
		// check if the cells are the same
		if (protozoa_cell_index == id)
			continue;

		const float pos_b_x = render_data_.positions_x[id];
		const float pos_b_y = render_data_.positions_y[id];

		const float diff_x = pos_a_x - pos_b_x;
		const float diff_y = pos_a_y - pos_b_y;

		const float dist_sq = diff_x * diff_x + diff_y * diff_y;
		const float local_diam = render_data_.radii[protozoa_cell_index] + render_data_.radii[id];

		if (dist_sq > local_diam * local_diam)
			continue;

		// Cells are not the same & they are intersecting
		const float dist = std::sqrt(dist_sq);
		if (dist == 0.0f) // Prevent division by zero
			continue;

		// Compute the overlap
		const float overlap = local_diam - dist;

		// Compute the collision normal
		const sf::Vector2f collisionNormal = sf::Vector2f(diff_x, diff_y) / dist;

		// Move the cells apart
		collision_resolutions[protozoa_cell_index] = collisionNormal * (overlap * 0.5f);
		collision_resolutions[id] = -collisionNormal * (overlap * 0.5f);
	}
}

void World::build_color_groups()
{
	for (auto& g : collision_color_groups) g.clear();

	for (int y = 0; y < spatial_hash_grid_.CellsY; ++y)
		for (int x = 0; x < spatial_hash_grid_.CellsX; ++x)
		{
			const int color = (x % 3) + 3 * (y % 2);
			const int cell_id = y * spatial_hash_grid_.CellsX + x;
			collision_color_groups[color].push_back(cell_id);
		}
}