#pragma once
#include <toml++/toml.hpp>
#include <cstdint>

struct FoodManagerSettings
{
	inline static uint32_t pheromone_grid_power;
	inline static uint16_t cell_max_capcity;

	inline static size_t update_freq; // food do not move that often so they dont have to be updated in the grid every frame

	inline static unsigned max_food;
	inline static unsigned initial_food;

	inline static float friction;

	inline static float kFoodVisibilityRampFrames;
	inline static float kFoodMaxAlpha;

	inline static int food_random_spawn_per_frame;
	inline static float food_random_spawn_chance;

	inline static float spawn_proportionality_constant; // range between [0.001, 0.01]
	inline static float food_spawn_distance;

	inline static float death_age_chance; // every frame past its death age gives it this chance of dying
	
	inline static float fade_start_nutrients;       // nutrients level at which fading begins

	inline static float food_launch_strength;
	inline static float food_launch_chance;

	inline static int pheromone_update_freq = 60;
	inline static int pheromone_render_update_freq = 30;

	// drag
	inline static constexpr float kDragLinearCoeff = 0.02f; // linear drag coefficient

	inline static constexpr int radius_update_interval = 60; // update radius every N frames
};
