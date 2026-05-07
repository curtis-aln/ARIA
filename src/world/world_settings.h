#pragma once
#include <cstdint>

struct WorldSettings
{
	inline static float bounds_radius;

	inline static uint32_t cells_x;
	inline static uint32_t cells_y;
	inline static uint32_t cell_max_capacity;

	inline static float border_repulsion_magnitude; // how strong it is repelled from the border
	inline static int updating_threads = 15;
};
