#pragma once
#include "Utils/Graphics/font_renderer.hpp"
#include <cstdint>

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