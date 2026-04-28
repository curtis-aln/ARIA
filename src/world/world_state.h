#pragma once
#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Color.hpp>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  WorldToggles
//  Owned by the main thread (ImGui writes, update thread reads).
//  Copied into SharedState each frame before the update thread reads it.
// ─────────────────────────────────────────────────────────────────────────────
struct WorldToggles
{
    bool simple_mode = false;  // only render outer circles of cells
    bool debug_mode = false;  // show per-cell debug info
    bool skeleton_mode = false;  // only render springs, no cell bodies
    bool paused = false;  // pause the simulation update loop
    bool draw_cell_grid = false;  // render the cell spatial hash grid
    bool draw_food_grid = false;  // render the food spatial hash grid
    bool toggle_collisions = true;   // enable/disable cell collision handling
    bool show_connections = true;   // show spring connections between cells
    bool show_bounding_boxes = true;  // show protozoa bounding boxes
    bool track_statistics = true;   // gather per-frame statistics
	bool m_tick_frame_time = false;  // whether to advance the simulation by one tick (for debugging)
	bool  m_rendering_ = true; // whether to render the simulation (for debugging)
	bool  hide_panels = false; // whether to hide ImGui panels (for recording clean screenshots)
	bool track_spatial_grids = false;  // gather spatial grid statistics each frame
	bool  open_extinction_window = false; // whether to open the extinction confirmation popup

    float min_speed = 0.f;
    float delta_min_speed = 0.f;
};

// ─────────────────────────────────────────────────────────────────────────────
//  WorldStatistics
//  Owned by the update thread (sim writes every tick).
//  Copied into the snapshot so ImGui can read it safely.
// ─────────────────────────────────────────────────────────────────────────────
struct WorldStatistics
{
    int   protozoa_count = 0;
    int   food_count = 0;
    int   peak_protozoa_ever = 0;
    int   highest_generation_ever = 0;
	int   most_offspring_ever = 0;
	int   entity_count = 0;

    float average_generation = 0.f;
    float average_cells_per_protozoa = 0.f;
    float average_offspring_count = 0.f;
    float average_mutation_rate = 0.f;
    float average_mutation_range = 0.f;
    float average_energy = 0.f;
    float average_spring_count = 0.f;
    float genetic_diversity = 0.f;
    float energy_efficiency = 0.f;
	float average_lifetime = 0.f; 

	float births_per_hundered_frames = 0.f;
	float deaths_per_hundered_frames = 0.f;
	float infant_mortality_rate = 0.f;
	float longest_lived_ever = 0.f;

    float frames_per_generation = -1.f; // negative = undefined
    float tracked_generation = 0.f;
    float frames_since_last_gen_change = 0.f;

    float fps = 0.f;
	float updating_fps = 0.f;
    int iterations_ = 0;
    float m_total_time_elapsed_ = 0.f;

    std::vector<float> gen_data{};
};

// ─────────────────────────────────────────────────────────────────────────────
//  RenderData
//  Written by the update thread, read by the render thread.
//  Contains everything needed to draw the simulation without touching
//  live simulation objects.
// ─────────────────────────────────────────────────────────────────────────────
struct RenderData
{
    alignas(64) std::vector<float> positions_x;
    alignas(64) std::vector<float> positions_y;
    alignas(64) std::vector<sf::Color>    outer_colors;
    alignas(64) std::vector<sf::Color>    inner_colors;
    alignas(64) std::vector<float>        radii;
    int                       entity_count = 0;

    void reserve(const int max_cells)
    {
        positions_x.resize(max_cells);
        positions_y.resize(max_cells);
        outer_colors.resize(max_cells);
        inner_colors.resize(max_cells);
        radii.resize(max_cells);
    }
};


struct SpatialGridData
{
	uint32_t cells_x;
	uint32_t cells_y;
	uint32_t cell_max_capacity;
	float world_width;
	float world_height;

    float cell_width;
	float cell_height;

    int total = 0, max_in = 0, full = 0, empty = 0; 
    float inv = 0.f;
};


struct FoodData
{
	std::vector<float> positions_x;
	std::vector<float> positions_y;
	std::vector<sf::Color> colors;
	std::vector<float> radii;
    size_t active_count = 0;
};