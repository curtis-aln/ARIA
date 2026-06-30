#pragma once

#include <SFML/Graphics.hpp>

#include "../../entities/food/food.h"

#include "../../Utils/Graphics/CircleBatchRenderer.h"
#include "../../Utils/o_vec/o_vector.hpp"
#include "food_manager_settings.h"
#include "../../Utils/spatial_grid/simple_spatial_grid.h"
#include "../../Utils/spatial_grid/spatial_grid_renderer.h"
#include "food_data.h"
#include "world/world_border.h"

struct FoodBodyPair
{
    Food* food_ptr;
    Body* body_ptr;

    bool is_valid()
    {
        return (food_ptr != nullptr) && (body_ptr != nullptr);
    }
};


struct SimSnapshot;

class FoodManager : public FoodManagerSettings
{
    sf::RenderWindow* window_;
    WorldBorder* world_bounds_;

    CircleBatchRenderer food_renderer;

    o_vector<Body>* bodies_;
    o_vector<Food> food_vector{ max_food };

	FoodData food_data{};

public:
    SimpleSpatialGrid spatial_hash_grid;
    SpatialGridRenderer food_grid_renderer;

    int frames = 0;

public:
    FoodManager(sf::RenderWindow* window, WorldBorder* world_bounds, o_vector<Body>* bodies);
    void  init();

    int    get_size()               const;
    void update_food_grid_renderer();
    void fill_data(FoodData& other_food_data);
    const o_vector<Food>& get_food_vector() const;
    o_vector<Food>& get_food_vector();
    void   update();
    void   render(const FoodData& snapshot_food_data);
    void update_position_data();
    void   remove_food(int food_id);
    Food* at(int idx);
    const Food* at(int idx) const;
    void   draw_food_grid(sf::Vector2f mouse_pos) const;

    FoodBodyPair create_food(const sf::Vector2f& position, bool random_genetics);

private:
    void  update_food();
    static void  update_food_nutrients(Food* food);
    void update_food_size(Food* food, Body* body);
    void  vibrate_food(Body* body, float strength);
    
    void  check_food_death(const Food* food);
    void  add_food_to_hash_grid();

    void  let_food_reproduce();
    bool  reproduce_food(Food* food);
    
    float calculate_spawn_chance() const;
    static bool can_food_reproduce(const Food* food) { return food->time_since_last_reproduced >= reproductive_cooldown && food->age >= reproductive_threshold;}

    bool  food_container_full() { return food_vector.size() >= max_food; }
};
