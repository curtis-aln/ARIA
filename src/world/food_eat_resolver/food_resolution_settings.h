#pragma once
#include <cstdint>

struct FoodResolutionSettings
{
	inline static uint32_t cells_x = (1u << 6); // for morton indexing, must be a power of 2
	inline static uint32_t cells_y = cells_x;     // square worlds
	inline static const uint32_t cell_max_capacity = 6; // maximum number of particles per cell, must be less than 256, but really shouldnt be any greater than 6
	inline static const int threads = 16;
};
