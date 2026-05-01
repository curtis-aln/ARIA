#pragma once

#include <SFML/Graphics.hpp>
#include "Food.h"
#include "../Utils/Graphics/CircleBatchRenderer.h"
#include "../Utils/o_vector.hpp"
#include "../Utils/Graphics/Circle.h"
#include "../settings.h"
#include "../Utils/Graphics/spatial_grid/simple_spatial_grid.h"
#include "../Utils/Graphics/spatial_grid/spatial_grid_renderer.h"
#include "food_data.h"

struct SimSnapshot;

class FoodManager : FoodSettings
{
    sf::RenderWindow* window_;
    Circle* world_bounds_ = nullptr;

    CircleBatchRenderer food_renderer;
    o_vector<Food> food_vector{ max_food };

	FoodData food_data{};

public:
    const float           bounds_radius = world_bounds_->radius;
    const sf::FloatRect   world_rect_bounds = { {0, 0}, {bounds_radius * 2, bounds_radius * 2} };

    SimpleSpatialGrid spatial_hash_grid{ cells_x, cells_y, cell_max_capacity, bounds_radius * 2, bounds_radius * 2 };
    SpatialGridRenderer food_grid_renderer{ &spatial_hash_grid };

    int frames = 0;

public:
    FoodManager(sf::RenderWindow* window, Circle* world_circular_bounds);

    int    get_size()               const;
    void update_food_grid_renderer();
    void fill_data(FoodData& other_food_data);
    const o_vector<Food>& get_food_vector() const;
    void   update();
    void   render(const FoodData& snapshot_food_data);
    void update_position_data();
    void   remove_food(int food_id);
    Food* at(int idx);
    const Food* at(int idx) const;
    void   draw_food_grid(sf::Vector2f mouse_pos) const;

private:
    void  init_food();
    void  update_food();
    static void  update_food_nutrients(Food* food);
    static void  vibrate_food(Food* food, float strength);
    
    void  bound_food_to_world(Food* food) const;
    void  check_food_death(const Food* food);
    void  update_hash_grid();

    static void  spawn_food();
    void  spawn_food_improved();
    bool  reproduce_food(Food* food);
    float calculate_spawn_chance() const;
    static bool  can_food_reproduce(const Food* food);
    bool  food_container_full() const;
};
