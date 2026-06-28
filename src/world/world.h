#pragma once

#include "../managers/food_manager/food_manager.h"
#include "../managers/cell_manager/cell_manager.h"

#include <SFML/Graphics/RenderWindow.hpp>

#include "world_settings.h"
#include "world_state.h"
#include "../managers/cell_manager/cell_manager_settings.h"

#include "collision/food_claim_buffer.h"

#include "../Utils/thread_pool.h"

#include "../Utils/Graphics/CircleBatchRenderer.h"
#include "../Utils/spatial_grid/simple_spatial_grid.h"
#include "../Utils/spatial_grid/spatial_grid_renderer.h"
#include "../simulation/context/sim_snapshot.h"
#include "managers/cell_manager/organism_tracker.h"

#include "Utils/fps_manager.h"
#include "Utils/Graphics/font_renderer.hpp"




class World : public WorldSettings
{
    sf::RenderWindow* m_window_ = nullptr;

    WorldBorder        world_circular_bounds_{ {bounds_radius, bounds_radius}, bounds_radius };
    sf::FloatRect world_rect_bounds_{ {0.f, 0.f}, {bounds_radius * 2.f, bounds_radius * 2.f} };
    sf::VertexArray world_border_renderer_{};

    // Render data — written each update tick, read by the renderer.
    RenderData render_data_;

    // these vectors are used for collision resolution
    alignas(64) std::vector<sf::Vector2f> entity_positions_;
    alignas(64) std::vector<sf::Vector2f> entity_velocities_;
    alignas(64) std::vector<float> entity_radii_;

    // Statistics accumulated each tick by the update thread.
    WorldStatistics statistics_{};

    // for the physics updating 
    // both food and cells query id's from this vector
	o_vector<Body> bodies_{ max_circles + FoodManagerSettings::max_food };

    float tex_rad = 120;
    CircleBatchRenderer outer_circle_renderer_{};
    CircleBatchRenderer inner_circle_renderer_{};
    std::vector<float>  inner_radii_{};

    FoodManager        food_manager_{ m_window_, &world_circular_bounds_, &bodies_ };
	CellManager 	  cell_manager_{ m_window_ , &world_circular_bounds_, &bodies_ };

    SimpleSpatialGrid  spatial_hash_grid_{ cells_x, cells_y, cell_max_capacity,
                                           bounds_radius * 2.f, bounds_radius * 2.f };
    SpatialGridRenderer cell_grid_renderer_{ &spatial_hash_grid_ };

    BarrierThreadPool collision_thread_pool_{ updating_threads };
	BarrierThreadPool food_thread_pool_{ updating_threads };
    std::vector<float> distribution_{};

    uint8_t max_capacity_area = cell_max_capacity * 9;
    static thread_local FixedSpan<uint32_t> tl_nearby_ids;
    static thread_local FixedSpan<obj_idx> tl_nearby_food;

    int max_circles = CellManagerSettings::max_protozoa;

    std::vector<int> colour_job_boundaries_;

    // Generation tracking (internal — summarised into statistics_)
    float tracked_generation_ = 0.f;
    float frames_since_last_gen_change_ = 0.f;

    FrameRateSmoothing<30> frame_rate_smoothing_{};

    // for multithreadded collision resolution
    std::array<std::vector<int>, 6> collision_color_groups;
    FoodClaimBuffer claim_buffer;

    std::vector<std::function<void()>> collision_jobs_;
    std::vector<std::function<void()>> food_jobs_;

    // every frame this is filled with the collision resolutions calculated in the collision detection phase, and then applied to the cells in the update phase. 
    // this is done to avoid modifying cell velocities during the collision detection phase which can cause errors in subsequent collision checks within the same frame.
    //alignas(64) std::vector<sf::Vector2f> collision_resolutions{};
    alignas(64) std::vector<sf::Vector2f> collision_resolutions{};
    alignas(64) std::vector<sf::Vector2f> velocity_resolutions{};

	OrganismTracker protozoa_tracker_{};

public:
    // ── Toggles — written by ImGui (main thread), read by update thread ──────
    // Safe to read/write without locking while the threads are not simultaneously
    // accessing them; copy into SharedState before handing to the update thread.
    WorldToggles toggles;

public:
    explicit World(sf::RenderWindow* window = nullptr);
    void init_circle_renderers();

    // ── Update ───────────────────────────────────────────────────────────────
    void update();
    void update_bodies();
    void bound_body_to_world(Body* body);
    void init_collision_jobs();
    void calculate_collision_resolutions();
    void resolve_collisions();

    // ── Render ───────────────────────────────────────────────────────────────
    void render(const SimSnapshot& snapshot, Font* font, sf::Vector2f mouse_pos);

    // ── Accessors — spatial grids / food ─────────────────────────────────────
    SimpleSpatialGrid* get_spatial_grid() { return &spatial_hash_grid_; }
    SimpleSpatialGrid* get_food_spatial_grid() { return &food_manager_.spatial_hash_grid; }
    const SimpleSpatialGrid* get_spatial_grid() const { return &spatial_hash_grid_; }
    const SimpleSpatialGrid* get_food_spatial_grid() const { return &food_manager_.spatial_hash_grid; }
    FoodManager* get_food_manager() { return &food_manager_; }
    const FoodManager* get_food_manager() const { return &food_manager_; }
    const CellManager* get_cell_manager() const { return &cell_manager_; }
    void               update_spatial_renderers();

    // world.h
    void update_cells_in_grid_cell(int grid_cell_id, FixedSpan<uint32_t>& nearby_ids, int thread_id);
    void update_protozoa_cell(int protozoa_cell_index, const FixedSpan<uint32_t>& nearby_ids, int thread_id);

    void unload_render_data(SimSnapshot& snapshot);
    static SpatialGridData get_grid_data(SimpleSpatialGrid* grid);
    void calculate_spatial_grid_statistics(SimpleSpatialGrid* grid, SpatialGridData& data);

    void fill_snapshot(SimSnapshot& snapshot);


	Cell* at(const int idx) { return cell_manager_.all_cells_.at(idx); }
    const Cell* at(const int idx) const { return cell_manager_.all_cells_.at(idx); }

    // ── Render data getters — read by renderer from snapshot ─────────────────
    const std::vector<float>& get_positions_x()    const { return render_data_.positions_x; }
    const std::vector<float>& get_positions_y()    const { return render_data_.positions_y; }
    const std::vector<sf::Color>& get_outer_colors() const { return render_data_.outer_colors; }
    const std::vector<sf::Color>& get_inner_colors() const { return render_data_.inner_colors; }
    const std::vector<float>& get_radii()        const { return render_data_.radii; }
    int                       get_entity_count() const { return statistics_.entity_count; }
    const RenderData& get_render_data()  const { return render_data_; }

    // ── Statistics getters — read by ImGui from snapshot ─────────────────────
    const WorldStatistics& get_statistics()  const { return statistics_; }
    int   get_food_count()                   const { return food_manager_.get_size(); }

    void render_springs(const SimSnapshot& snapshot);

    // ── Selection ─────────────────────────────────────────────────────────────
    bool handle_mouse_click(sf::Vector2f mouse_position);
    void keyboardEvents(const sf::Keyboard::Key& event_key_code);

    // ── Generation distribution (for histogram) ───────────────────────────────
    const std::vector<float>& get_generation_distribution();

private:
    void copy_render_data_to_snapshot(SimSnapshot& snapshot);
    void copy_spatial_grids_to_snapshot(SimSnapshot& snapshot);

    // Rendering
    void draw_protozoa_debug(const SimSnapshot& snapshot, Font* font);
    void draw_cell_outlines(const OrganismTracker& protozoa);
    void draw_cell_physical_information(const OrganismTracker& protozoa, Font* font) const;
    void draw_spring_information(Font* font) const;

    void nearby_food_information(const OrganismTracker& protozoa) const;

    int check_mouse_press(const OrganismTracker& protozoa, sf::Vector2f mousePosition, bool tolerance_check) const;
   

    void build_color_groups();
    void update_nearby_container(int32_t neighbour_index_x, int32_t neighbour_index_y, FixedSpan<uint32_t>& nearby_ids);

    void update_position_container();
    void update_statistics();

    void calculate_averaging_statistics();

    void render_protozoa(const SimSnapshot& snapshot, Font* font);
    void init_food_jobs();
    void resolve_food_interactions();
    void resolve_food_interactions_threadded();
    void resolve_food_grid_cell(int cell_id, FixedSpan<obj_idx>& nearby_food);
};
