#pragma once

#include <SFML/Graphics.hpp>

#include "../../entities/food/food.h"

#include "../../Utils/Graphics/CircleBatchRenderer.h"
#include "../../Utils/o_vector.hpp"
#include "food_manager_settings.h"
#include "../../Utils/spatial_grid/simple_spatial_grid.h"
#include "../../Utils/spatial_grid/spatial_grid_renderer.h"
#include "food_data.h"
#include "world/world_border.h"

struct SimSnapshot;

class FoodManager : public FoodManagerSettings
{
    sf::RenderWindow* window_;
    WorldBorder* world_bounds_;

    CircleBatchRenderer food_renderer;
    o_vector<Food> food_vector{ max_food };

	FoodData food_data{};

public:
    SimpleSpatialGrid spatial_hash_grid;
    SpatialGridRenderer food_grid_renderer;

    int frames = 0;

public:
    FoodManager(sf::RenderWindow* window, WorldBorder* world_bounds);

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
