#pragma once
#include "Utils/Graphics/font_renderer.hpp"
#include <cstdint>
#include "Protozoa/genetics/CellGenome.h"

void load_settings(const std::string& path);


struct SimulationSettings
{
	inline static constexpr int frame_smoothing = 30;
	inline static constexpr double resize_shrinkage = 0.95;
	inline static const std::string simulation_name = "Project A.R.I.A";
	inline static const std::string settings_file_location = "media/aria_settings.toml";

	inline static constexpr float camera_lerp_factor = 0.04f; // how quickly the camera follows the selected protozoa

	inline static bool full_screen;
	inline static bool vsync;

	inline static unsigned max_fps;

	inline static float ui_scale_percent;
};

struct GraphicalSettings
{
	inline static const sf::Color window_color = { 0, 0, 0 };


	inline static float spring_thickness;
	inline static float spring_outline_thickness;

	inline static float cell_outline_thickness; // relative to the size of the cell

	inline static std::uint8_t food_transparency;

	inline static const std::vector<sf::Color> food_fill_colors = {
		{200, 30, 30}
	};

	inline static const std::vector<sf::Color> food_outline_colors = {
		{250, 60, 60}
	};
};


struct WorldSettings
{
	inline static float bounds_radius;

	inline static unsigned max_protozoa;
	inline static unsigned initial_protozoa;

	inline static uint32_t cells_x;
	inline static uint32_t cells_y;
	inline static uint32_t cell_max_capacity;

	inline static float border_repulsion_magnitude; // how strong it is repelled from the border
	inline static float max_speed;

	inline static bool auto_reset_on_extinction = false;

	inline static int updating_threads = 15;
};

struct ProtozoaSettings
{
	inline static float offspring_energy_cost = 400.f;
	inline static float reproduce_energy_thresh = 400.f;

	inline static int max_cells;

	inline static float spawn_radius;

	inline static float energy_decay_rate; // how quickly energy decays per frame

	inline static float wander_threshold; // if a cell wanders too far away from the protozoa it kills the whole thing

	inline static float breaking_length;
	inline static float maximum_extension;

	inline static float spring_work_const; // how we scale the energy cost of springs

	inline static size_t reproductive_cooldown;
	inline static float digestive_time; // per cell

	inline static float initial_energy; // energy the protozoa spawn with

};


struct FoodSettings
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


struct TextSettings
{
	// locations for the fonts
	inline static const std::string bold_font_location = "media/fonts/Roboto-Bold.ttf";
	inline static const std::string regular_font_location = "media/fonts/Roboto-Regular.ttf";

	// Universal font sizes - only these many fonts should be created
	static constexpr unsigned title_font_size = 30;
	static constexpr unsigned regular_font_size = 20;
	static constexpr unsigned cell_statistic_font_size = 15;

};