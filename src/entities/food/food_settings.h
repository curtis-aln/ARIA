#pragma once

struct FoodSettings
{
	inline static uint16_t repro_cooldown;
	inline static float nutrient_reproductive_threshold;

	inline static float initial_nutrients;
	inline static float final_nutrients;
	inline static uint16_t nutrient_development_time;

	inline static float spawn_immunity;

	inline static float vibrate_freq;

	inline static float death_age;

	inline static float food_offspring_spawn_distance;

	inline static uint8_t   outer_transparency = 125;
	inline static uint8_t   inner_transparency = 100;

	inline static sf::Vector3i food_darkest_color = { 0, 225, 0 };
	inline static sf::Vector3i food_lightest_color = { 80, 255, 100 };

	inline static float nutrients_to_mass_scale; // radius = nutrients * this constant

	inline static constexpr int vibration_frequency = 160; // how many iterations between vibrations
	inline static float vibration_strength;
};