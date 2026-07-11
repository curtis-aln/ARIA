#pragma once
#include <SFML/Graphics/Color.hpp>
#include <vector>
#include "../Utils/o_vec/o_vector.hpp"
#include "../Utils/o_vec/o_vec_snapshot.h"
#include "../entities/cell/cell.h"
#include "../entities/food/food.h"
#include "../entities/body.h"
#include "../entities/spring/spring.h"
#include "world/connection_renderer.h"


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
    bool run_physics_only = true;

    // Mouse tool — written by UI, read by handle_left_click()

    bool  mouse_add_cells = true;
    bool  mouse_add_food = false;
    bool  mouse_rem_cells = true;
    bool  mouse_rem_food = false;
    

	bool show_influence_radius = false; // whether to show the influence radius of the mouse tool
};

// ─────────────────────────────────────────────────────────────────────────────
//  WorldStatistics
//  Owned by the update thread (sim writes every tick).
//  Copied into the snapshot so ImGui can read it safely.
// ─────────────────────────────────────────────────────────────────────────────
struct WorldStatistics
{
    int   cell_count = 0;
    int   food_count = 0;

    int   peak_protozoa_ever = 0;
    int   highest_generation_ever = 0;
	int   most_offspring_ever = 0;
	
    int container_size_read = 0;

    float average_generation = 0.f;
    float average_cells_per_protozoa = 0.f;
    float average_offspring_count = 0.f;
    float average_mutation_rate = 0.f;
    float average_mutation_range = 0.f;
    float average_energy = 0.f;
    float average_spring_count = 0.f;
    float energy_efficiency = 0.f;
	float average_lifetime = 0.f; 

	float births_per_hundered_frames = 0.f;
	float deaths_per_hundered_frames = 0.f;
	float infant_mortality_rate = 0.f;

    int   total_deaths = 0;
    int   infant_deaths = 0;
    int deaths_this_window = 0;
    int births_this_window = 0;

    uint16_t longest_lived_ever = 0;

    float frames_per_generation = -1.f; // negative = undefined
    float tracked_generation = 0.f;
    float frames_since_last_gen_change = 0.f;

    int iterations_ = 0;

    int   mouse_mode = 0;      // 0 = Add, 1 = Remove
    int mouse_intensity = 1;
    float mouse_radius = 300.f;

    int highlighted_cells = 0;
    int highlighted_food = 0;

    std::vector<float> gen_data{};

    // for springs
    float spring_breaking_length = 0.f;
	float spring_breaking_force = 0.f;
	float spring_work_const = 0.f;
	float spring_damage_threshold = 0.f;
};

// ─────────────────────────────────────────────────────────────────────────────
//  RenderData
//  Written by the update thread, read by the render thread.
//  Contains everything needed to draw the simulation without touching
//  live simulation objects.
// ─────────────────────────────────────────────────────────────────────────────
struct RenderData
{
    alignas(64) std::vector<sf::Vector2f> positions;
    alignas(64) std::vector<sf::Vector2f> velocities;
    alignas(64) std::vector<sf::Color>    outer_colors;
    alignas(64) std::vector<sf::Color>    inner_colors;
    alignas(64) std::vector<float>        radii;


	std::vector<Connection> spring_connections;

    OVecDebugImGuiSnapshot cell_debug_snapshot;
    OVecDebugImGuiSnapshot food_debug_snapshot;
    OVecDebugImGuiSnapshot body_debug_snapshot;
    OVecDebugImGuiSnapshot spring_debug_snapshot;

    // for

    void reserve(const int max_cells)
    {
        positions.resize(max_cells);
        velocities.resize(max_cells);
        outer_colors.resize(max_cells);
        inner_colors.resize(max_cells);
        radii.resize(max_cells);
        spring_connections.resize(max_cells);
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
