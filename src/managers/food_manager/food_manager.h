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
#include "../../simulation/context/state.h"

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
    
	FoodManagerStatistics statistics_{};

public:
    FixedSpan<cell_idx, uint16_t> select_indexes{ static_cast<uint16_t>(10000) };

    int frames = 0;

public:
    FoodManager(sf::RenderWindow* window, WorldBorder* world_bounds, o_vector<Body>* bodies);
    void  init();

    void reset();

    void remove_food_in_area(const sf::Vector2f& center, float radius);

    void gather_food_in_radius(FixedSpan<cell_idx, uint16_t>& indexes, const sf::Vector2f& position, const float radius);

    void influence_food_velocities_in_radii(const sf::Vector2f& position, const float radius, const int intensity);

    int    get_size()               const;
    bool has_food_with_body_id(int body_id);
    void fill_data(FoodData& other_food_data);
    const o_vector<Food>& get_food_vector() const;
    o_vector<Food>& get_food_vector();
    void   update();
    void   render(const FoodData& snapshot_food_data);
    void update_position_data();
    void   remove_food(int food_id);
    Food* at(int idx);
    const Food* at(int idx) const;

    FoodBodyPair create_food(const sf::Vector2f& position, bool random_genetics);

	FoodManagerStatistics& get_statistics() { return statistics_; }

private:
    void  update_food();
    void update_statistics();
    
    void  check_food_death(const Food* food);

    void  let_food_reproduce();
    bool  reproduce_food(Food* food);
    
    float calculate_spawn_chance() const;
    static bool can_food_reproduce(const Food* food) { return food->time_since_last_reproduced >= reproductive_cooldown && food->age >= reproductive_threshold;}

    bool  food_container_full() { return food_vector.size() >= max_food; }
};
