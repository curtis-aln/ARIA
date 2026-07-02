#pragma once
#include <cstdint>

struct WorldSettings
{
	inline static float bounds_radius;

	inline static uint32_t cells_x;
	inline static uint32_t cells_y;
	inline static uint32_t cell_max_capacity;

	inline static float border_repulsion_magnitude; // how strong it is repelled from the border
	inline static unsigned int updating_threads = 16;

	inline static constexpr float grid_line_thickness = 8.5f;
	inline static const sf::Color grid_color = { 75, 75, 75, 100 };

	inline static const float start_fading_zoom = 0.1f;
	inline static const float fade_zoom_dist = 0.02f; // grid will be fully invisible at start_fading_zoom + fade_zoom_dist
};
