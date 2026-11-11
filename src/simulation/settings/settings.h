#pragma once
#include <SFML/Graphics/Color.hpp>
#include <string>
#include <vector>

void load_settings(const std::string& path);


struct BackgroundPreset
{
	std::string name;
	std::vector<sf::Color> colors; // gradient stops, same layout as bg_colors
};


struct SimulationSettings
{
	inline static int max_iterations;
	inline static float max_simulation_time;

	inline static double initial_frame_rate_updating; // set to zero to run the simulation as fast as possible
	inline static double initial_frame_rate_rendering;

	inline static constexpr int frame_smoothing = 60;
	inline static constexpr double window_resize_shrinkage = 0.95;

	inline static const std::string simulation_name = "Project A.R.I.A";
	inline static const std::string settings_file_location = "media/aria_settings.toml";

	inline static constexpr float camera_lerp_factor = 0.04f; // how quickly the camera follows the selected protozoa

	// colors
	inline static std::vector<sf::Color> bg_colors = { {0, 0, 0}, { 0, 5, 40 } };
	inline static sf::Color window_color = { 0, 0, 0 };

	inline static const std::vector<BackgroundPreset> bg_presets = {
	{ "Deep Space", { {0, 0, 0},   {0, 5, 40} } },
	{ "Void Black", { {0, 0, 0},   {0, 0, 0} } },
	{ "Ocean",      { {0, 10, 20}, {0, 40, 60} } },
	{ "Twilight",   { {10, 0, 20}, {40, 5, 60} } },
	{ "Custom",     {} } // populated live from the color pickers
	};

	inline static int bg_preset_index = 0; // index into bg_presets currently active

	inline static bool full_screen;
	inline static bool vsync;
	inline static bool set_seed = true;

	inline static float ui_scale_percent;

	inline static constexpr float min_zoom_to_select_protozoa = 0.062f; // if zoomed out more than this, clicking on protozoa is disabled

};
