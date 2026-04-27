#pragma once

#include <SFML/Graphics/RenderWindow.hpp>

#include "../Protozoa/Protozoa.h"
#include "../Food/food_manager.h"
#include "../settings.h"
#include "ProtozoaManager.h"
#include "world_state.h"
#include "food_claim_buffer.h"

#include "../Utils/thread_pool.h"
#include "../Utils/o_vector.hpp"
#include "../Utils/Graphics/Circle.h"
#include "../Utils/Graphics/CircleBatchRenderer.h"
#include "../Utils/Graphics/spatial_grid/simple_spatial_grid.h"
#include "../Utils/Graphics/spatial_grid/spatial_grid_renderer.h"
#include "../simulation/sim_snapshot.h"
#include "Utils/fps_manager.h"


class World : public ProtozoaManager
{
    sf::RenderWindow* m_window_ = nullptr;

    Circle        world_circular_bounds_{ {bounds_radius, bounds_radius}, bounds_radius };
    sf::FloatRect world_rect_bounds_{ {0.f, 0.f}, {bounds_radius * 2.f, bounds_radius * 2.f} };
    sf::VertexArray world_border_renderer_{};

    // Render data — written each update tick, read by the renderer.
    RenderData render_data_;

    std::vector<Cell*> cell_pointers_{};

    size_t max_circles = max_protozoa * ProtozoaSettings::max_cells;
    float tex_rad = 120;
    CircleBatchRenderer outer_circle_renderer_{ m_window_, tex_rad, max_circles };
    CircleBatchRenderer inner_circle_renderer_{ m_window_, tex_rad, max_circles };
    std::vector<float>  inner_radii_{};

    FoodManager        food_manager_{ m_window_, &world_circular_bounds_ };
    SimpleSpatialGrid  spatial_hash_grid_{ cells_x, cells_y, cell_max_capacity,
                                           bounds_radius * 2.f, bounds_radius * 2.f };
    SpatialGridRenderer cell_grid_renderer_{ &spatial_hash_grid_ };

    BarrierThreadPool collision_thread_pool_{ updating_threads };
	BarrierThreadPool food_thread_pool_{ updating_threads };
    std::vector<float> distribution_{};

    uint8_t max_capacity_area = cell_max_capacity * 9;
    static thread_local FixedSpan<uint32_t> tl_nearby_ids;
    static thread_local FixedSpan<obj_idx> tl_nearby_food;

    // Statistics accumulated each tick by the update thread.
    WorldStatistics statistics_{};

    // Generation tracking (internal — summarised into statistics_)
    float tracked_generation_ = 0.f;
    float frames_since_last_gen_change_ = 0.f;

    FrameRateSmoothing<30> frame_rate_smoothing_{};

    // for multithreadded collision resolution
    std::array<std::vector<int>, 6> collision_color_groups;
    FoodClaimBuffer claim_buffer;

    std::vector<std::function<void()>> collision_jobs_;
    std::vector<std::function<void()>> food_jobs_;

public:
    // ── Toggles — written by ImGui (main thread), read by update thread ──────
    // Safe to read/write without locking while the threads are not simultaneously
    // accessing them; copy into SharedState before handing to the update thread.
    WorldToggles toggles;

    int entity_count = 0;

public:
    explicit World(sf::RenderWindow* window = nullptr);

    // ── Update ───────────────────────────────────────────────────────────────
    void update();
    void init_collision_jobs();
    void resolve_collisions_threaded();
    void resolve_collisions();

    // ── Render ───────────────────────────────────────────────────────────────
    void render(Font* font, sf::Vector2f mouse_pos);

    // ── Accessors — spatial grids / food ─────────────────────────────────────
    SimpleSpatialGrid* get_spatial_grid() { return &spatial_hash_grid_; }
    SimpleSpatialGrid* get_food_spatial_grid() { return &food_manager_.spatial_hash_grid; }
    FoodManager* get_food_manager() { return &food_manager_; }
    void               update_spatial_renderers();

    void unload_render_data(SimSnapshot& snapshot);
    static SpatialGridData get_grid_data(SimpleSpatialGrid* grid);
    void advanced_grid_data(SimpleSpatialGrid* grid, SpatialGridData& data);

    void fill_snapshot(SimSnapshot& snapshot);

    // ── Render data getters — read by renderer from snapshot ─────────────────
    const std::vector<float>& get_positions_x()    const { return render_data_.positions_x; }
    const std::vector<float>& get_positions_y()    const { return render_data_.positions_y; }
    const std::vector<sf::Color>& get_outer_colors() const { return render_data_.outer_colors; }
    const std::vector<sf::Color>& get_inner_colors() const { return render_data_.inner_colors; }
    const std::vector<float>& get_radii()        const { return render_data_.radii; }
    int                              get_entity_count() const { return entity_count; }
    const RenderData& get_render_data()  const { return render_data_; }

    // ── Statistics getters — read by ImGui from snapshot ─────────────────────
    const WorldStatistics& get_statistics()  const { return statistics_; }
    int   get_protozoa_count()               const { return all_protozoa_.size(); }
    int   get_food_count()                   const { return food_manager_.get_size(); }
    float get_average_generation()           const { return statistics_.average_generation; }

    // ── Selection ─────────────────────────────────────────────────────────────
    bool handle_mouse_click(sf::Vector2f mouse_position);
    void keyboardEvents(const sf::Keyboard::Key& event_key_code);

    // ── Generation distribution (for histogram) ───────────────────────────────
    const std::vector<float>& get_generation_distribution();

private:
    void update_cells_in_grid_cell(int grid_cell_id, FixedSpan<uint32_t>& nearby_ids);
    void update_protozoa_cell(int protozoa_cell_index, const FixedSpan<uint32_t>& nearby_ids);
    void build_color_groups();
    void update_nearby_container(int32_t neighbour_index_x, int32_t neighbour_index_y, FixedSpan<uint32_t>& nearby_ids);

    void update_position_container();
    void update_statistics();

    void render_protozoa(Font* font);
    void init_organisms();
    void init_food_jobs();
    void resolve_food_interactions();
    void resolve_food_interactions_threadded();
    void resolve_food_grid_cell(int cell_id, FixedSpan<obj_idx>& nearby_food);
};
