#pragma once
#include "../../Utils/o_vec/o_vec_snapshot.h"
#include "world/world_renderer/connection_renderer.h"
#include <cstdint>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>
#include <vector>
#include <vector>
#include <world/world_renderer/connection_renderer.h>


struct SimulationToggles
{
	bool  m_rendering_ = true; // whether to render the simulation (for debugging)

	// Imgui
	bool  hide_panels = false; // whether to hide ImGui panels (for recording clean screenshots)
};


// ─────────────────────────────────────────────────────────────────────────────
//  WorldToggles
//  Owned by the main thread (ImGui writes, update thread reads).
//  Copied into SharedState each frame before the update thread reads it.
// ─────────────────────────────────────────────────────────────────────────────
struct WorldToggles
{
	bool debug_mode = false;  // show per-cell debug info
	bool paused = true;  // pause the simulation update loop

	// grids
	bool draw_collision_grid = false;  // render the bodies spatial hash grid
	bool draw_food_grid = false;  // render the food spatial hash grid
	bool draw_background_grid = true;

	// debugging
	bool m_tick_frame_time = false;  // whether to advance the simulation by one tick (for debugging)

	// statistics
	bool track_statistics = true;   // gather per-frame statistics
	bool track_spatial_grids = false;  // gather spatial grid statistics each frame

	// physics
	bool run_physics_only = true;
	bool toggle_collisions = true;   // enable/disable cell collision handling

	// Mouse tool — written by UI, read by handle_left_click()
	bool  mouse_add_cells = true;
	bool  mouse_add_food = false;
	bool  mouse_rem_cells = true;
	bool  mouse_rem_food = false;

	bool show_influence_radius = false; // whether to show the influence radius of the mouse tool
};

struct CellManagerToggles
{
	// integrity damage conditions
	bool collision_integrity_damage_ = true;
	bool spring_stress_integrity_damage = true;

	bool spring_too_long_breakage = true;
	bool spring_too_much_force_breakage = false;

	bool work_done_energy = true;
	bool friction_energy_loss = true;
	bool old_age_death = true;

	// newborns
	bool show_only_newborns = false;
	bool show_newborn_grid = false;

	// debugging
	bool skeleton_mode = false;  // only render springs, no cell bodies
	bool show_connections = true;   // show spring connections between cells
	bool show_bounding_boxes = true;  // show protozoa bounding boxes

	// other
	bool easy_reproduction = true;  // cells can reproduce through hardcoded copy paste
};

struct FoodToggles
{
	bool spawn_random_food = true;
	bool food_mitosis = true;

	bool update_pheromone_grid = false;
	bool render_pheromone_grid = false;
};

// ─────────────────────────────────────────────────────────────────────────────
//  WorldStatistics
//  Owned by the update thread (sim writes every tick).
//  Copied into the snapshot so ImGui can read it safely.
// ─────────────────────────────────────────────────────────────────────────────
struct SimulationStatistics
{
	float max_frame_rate_updating = 0.f; // 0 = unlimited
	float max_frame_rate_rendering = 0.f;
	float total_time_elapsed = 0.f;

	float camera_zoom = 1.f;

	float mouse_pos_x = 0.f;
	float mouse_pos_y = 0.f;

	// Statistics
	float rendering_frame_rate = 0.f;
	float updating_frame_rate = 0.f;

};


struct WorldStatistics
{
	int   mouse_mode = 0;      // 0 = Add, 1 = Remove
	float mouse_intensity = 1;
	float mouse_radius = 300.f;

	int structure_mode = 0;

	float frames_per_generation = -1.f; // negative = undefined
	float tracked_generation = 0.f;
	float frames_since_last_gen_change = 0.f;

	int iterations_ = 0;

	int highlighted_cells = 0;
	int highlighted_food = 0;

	std::vector<float> gen_data{};

	float tracked_generation_ = 0.f;
	float frames_since_last_gen_change_ = 0.f;
};


struct CellManagerStatistics
{
	float min_speed = 0.f;

	uint16_t longest_lived_ever = 0;

	int   total_deaths = 0;  // increments whenever a cell dies
	int   cell_count = 0;

	int   peak_protozoa_ever = 0;
	uint32_t   highest_generation_ever = 0;
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

	float spring_breaking_length = 0.f;
	float spring_breaking_force = 0.f;
	float spring_work_const = 0.f;
	float spring_damage_threshold = 0.f;

	// hundered frame
	float births_per_hundered_frames = 0.f;
	float deaths_per_hundered_frames = 0.f;
	float non_repro_deaths_per_hundered_frames = 0.f;
	float infant_mortality_rate = 0.f;

	int infant_deaths_this_window = 0;
	int non_repro_deaths_this_window = 0;
	int deaths_this_window = 0;
	int births_this_window = 0;

};


struct FoodManagerStatistics
{
	int   food_count = 0;

	float pheromone_grid_cells_x = 0;
	float pheromone_grid_cells_y = 0;
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

	sf::Texture pheromone_texture;

	OVecDebugImGuiSnapshot cell_debug_snapshot;
	OVecDebugImGuiSnapshot food_debug_snapshot;
	OVecDebugImGuiSnapshot body_debug_snapshot;
	OVecDebugImGuiSnapshot spring_debug_snapshot;

	void reserve(const int max_cells)
	{
		positions.resize(max_cells);
		velocities.resize(max_cells);
		outer_colors.resize(max_cells);
		inner_colors.resize(max_cells);
		radii.resize(max_cells);
		spring_connections.resize(max_cells);
	}

	void clear_render_data()
	{
		positions.clear();
		velocities.clear();
		outer_colors.clear();
		inner_colors.clear();
		radii.clear();
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
