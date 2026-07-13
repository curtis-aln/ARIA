#pragma once

#include "../managers/food_manager/food_manager.h"
#include "../managers/cell_manager/cell_manager.h"

#include <SFML/Graphics/RenderWindow.hpp>

#include "world_settings.h"
#include "../simulation/context/state.h"
#include "../managers/cell_manager/cell_manager_settings.h"
#include "../managers/cell_manager/organism_tracker.h"

#include "collision_resolver/collision_resolver.h"

#include "../Utils/thread_pool.h"
#include "../Utils/Graphics/CircleBatchRenderer.h"
#include "../Utils/spatial_grid/simple_spatial_grid.h"
#include "../Utils/spatial_grid/spatial_grid_renderer.h"
#include "../simulation/context/sim_snapshot.h"
#include "../Utils/Graphics/font_renderer.hpp"
#include "../Utils/o_vec/o_vec_debug.h"
#include "../Utils/Graphics/SFML_Grid.h"

#include "world_renderer/world_renderer.h"


class World : public WorldSettings
{
    unsigned max_entities = CellManagerSettings::max_protozoa + FoodManagerSettings::max_food;

    sf::RenderWindow* m_window_ = nullptr;

    WorldBorder        world_circular_bounds_{ {bounds_radius, bounds_radius}, bounds_radius };
    sf::FloatRect world_rect_bounds_{ {0.f, 0.f}, {bounds_radius * 2.f, bounds_radius * 2.f} };
    sf::FloatRect visible_bounds = world_rect_bounds_;
   
    WorldStatistics statistics_{}; // Statistics accumulated each tick by the update thread.

    // for the physics updating 
    // both food and cells query id's from this vector
	o_vector<Body> bodies_{ max_entities };

    float tex_rad = 120;

    FoodManager        food_manager_{ m_window_, &world_circular_bounds_, &bodies_ };
	CellManager 	  cell_manager_{ m_window_ , &world_circular_bounds_, &bodies_ };

    sf::FloatRect bounds = { {0, 0}, {bounds_radius * 2, bounds_radius * 2} };
    
    CollisionResolver collision_resolver_{ &bounds, &bodies_, updating_threads, max_entities, max_entities };

	BarrierThreadPool food_thread_pool_{ (int)updating_threads };

    uint8_t max_capacity_area = cell_max_capacity * 9;
    static thread_local FixedSpan<uint32_t> tl_nearby_ids;
    static thread_local FixedSpan<obj_idx> tl_nearby_food;
    

    WorldRenderer world_renderer_{ m_window_, &food_manager_, &collision_resolver_, world_rect_bounds_, world_circular_bounds_ };

    std::vector<std::function<void()>> food_jobs_;

    // debugging o_vectors
    OVecDebug<Cell> dbg_cells_{ cell_manager_.get_all_cells()};
    OVecDebug<Food> dbg_food_{food_manager_.get_food_vector()};
	OVecDebug<Body> dbg_bodies_{bodies_};
	OVecDebug<Spring> dbg_springs_{cell_manager_.get_all_springs()};

public:
    // ── Toggles — written by ImGui (main thread), read by update thread ──────
    // Safe to read/write without locking while the threads are not simultaneously
    // accessing them; copy into SharedState before handing to the update thread.
    WorldToggles toggles;

    bool should_drag_protozoa_ = false;


public:
    explicit World(sf::RenderWindow* window = nullptr);

    // ── Update ───────────────────────────────────────────────────────────────
    void update(SimSnapshot& write_snapshot);

    // ── Render ───────────────────────────────────────────────────────────────
    void render(const SimSnapshot& snapshot, sf::Vector2f mouse_pos);

    void reset_world();


    // ── Accessors — spatial grids / food ─────────────────────────────────────
    SimpleSpatialGrid* get_spatial_grid() { return collision_resolver_.get_grid(); }
    SimpleSpatialGrid* get_food_spatial_grid() { return &food_manager_.spatial_hash_grid; }

    const SimpleSpatialGrid* get_food_spatial_grid() const { return &food_manager_.spatial_hash_grid; }
    FoodManager* get_food_manager() { return &food_manager_; }
    const FoodManager* get_food_manager() const { return &food_manager_; }
    const CellManager* get_cell_manager() const { return &cell_manager_; }
    CellManager* get_cell_manager() { return &cell_manager_; }
    void               update_spatial_renderers();
	int get_entity_count() const { return cell_manager_.get_cell_count() + food_manager_.get_size(); }

    // world.h

    void unload_render_data(SimSnapshot& snapshot);
    static SpatialGridData get_grid_data(SimpleSpatialGrid* grid);
    void calculate_spatial_grid_statistics(SimpleSpatialGrid* grid, SpatialGridData& data);

    

    // ── Statistics getters — read by ImGui from snapshot ─────────────────────
    WorldStatistics& get_statistics()  { return statistics_; }
    int   get_food_count()                   const { return food_manager_.get_size(); }

    void render_springs(const SimSnapshot& snapshot);

    // ── Selection ─────────────────────────────────────────────────────────────
    bool handle_mouse_click(sf::Vector2f mouse_position);
    void keyboardEvents(const sf::Keyboard::Key& event_key_code);

    void handle_right_click(WorldBorder& spawn_area);

private:
    void fill_snapshot(SimSnapshot& snapshot);

    void update_entities();
    void bound_bodies();
    void bound_body_to_world(Body* body);

    void copy_render_data_to_snapshot(SimSnapshot& snapshot);
    void copy_spatial_grids_to_snapshot(SimSnapshot& snapshot);

    void nearby_food_information(const OrganismTracker& protozoa) const;

    int check_mouse_press(const OrganismTracker& protozoa, sf::Vector2f mousePosition, bool tolerance_check) const;
   
    void debug_sanity_checks();

    sf::FloatRect calulcate_visible_range();

    void update_position_container_optimized(SimSnapshot& write_snapshot);

    void update_position_container(SimSnapshot& write_snapshot);
    void update_statistics();

    void resolve_food_interactions();
};
