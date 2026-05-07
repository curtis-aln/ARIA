#pragma once
#include <toml++/toml.hpp>
#include <cstdint>

struct FoodManagerSettings
{
	inline static uint32_t cells_x;
	inline static uint32_t cells_y;
	inline static uint32_t cell_max_capacity;
	inline static size_t update_freq; // food do not move that often so they dont have to be updated in the grid every frame

	inline static unsigned max_food;
	inline static unsigned initial_food;
	inline static float food_radius;
	inline static float friction;

	inline static sf::Vector3i food_darkest_color = { 0, 160, 0 };
	inline static sf::Vector3i food_lightest_color = { 80, 255, 100 };
	inline static float vibration_strength;

	inline static float kFoodVisibilityRampFrames;
	inline static float kFoodMaxAlpha;

	inline static float spawn_proportionality_constant; // range between [0.001, 0.01]
	inline static float food_spawn_distance;
	inline static size_t reproductive_cooldown;
	inline static float reproductive_threshold; // how old a food has to be before it can reproduce

	inline static float initial_nutrients;
	inline static float final_nutrients;
	inline static size_t nutrient_development_time;

	inline static float death_age;
	inline static float death_age_chance; // every frame past its death age gives it this chance of dying
};
