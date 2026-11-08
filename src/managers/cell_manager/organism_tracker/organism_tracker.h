#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <vector>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include "../../../simulation/context/state.h"

#include "../../../Utils/spatial_grid/simple_spatial_grid.h"
#include "../../../entities/food/food.h"
#include "../../../entities/cell/cell.h"
#include "../../../entities/spring/spring.h"
#include "Utils/o_vec/o_vector.hpp"

inline constexpr int neighbours_max = 100;


// A class dedicated to tracking statistics about a protozoa (an organism).
// It starts from the cell the user selected, walks the spring container to find every cell and
// spring connected to it, and uses those to calculate statistics about the whole organism.
//
// organism_tracker.cpp holds all the implementations.
class OrganismTracker
{
public:
    // Pass this to update_primitive() to mean "nothing is selected".
    static constexpr uint32_t invalid_cell_id = std::numeric_limits<uint32_t>::max();

    int  selected_id = -1;
    bool is_active = false;

    int original_selected_cell_id = -1;

    sf::FloatRect bounds{};

    sf::Vector2f position{};
    sf::Vector2f position_prev{};

    sf::Vector2f velocity{};
    sf::Vector2f velocity_prev{};

    std::vector<Cell>   cells{};
    std::vector<Spring> springs{};
    std::vector<Body>   bodies{};  // bodies[i] belongs to cells[i]

    sf::Vector2f acceleration{};
    float speed{};

    std::array<sf::Vector2f, neighbours_max> nearby_food_positions{};
    std::array<sf::Vector2f, neighbours_max> nearby_cell_positions{};
    int cells_in_neighbourhood = 0;
    int food_in_neighbourhood = 0;
    int total_in_neighbourhood = 0;

    float total_energy = 0.f;
    float average_energy = 0.f;
    float max_energy = 0.f;
    float spring_total_work_done = 0.f;

    float total_nutrients = 0.f;
    float max_nutrients = 0.f;

    float average_generation = 0.f;

    float frames_alive = 0.f;

    int cell_count = 0;
    int spring_count = 0;

    int offspring_count = 0;
    int time_since_last_reproduced = 0;
    int reproduction_cooldown = 0;
    int cells_ready_to_reproduce = 0;
    float ready_to_reproduce_percentage = 0.f;

    int total_food_eaten = 0;

    OrganismTracker() = default;

    // Rebuilds the organism from the selected cell and refreshes every statistic.
    void update_primitive(uint32_t selected_cell_id,
        const o_vector<Cell>& all_cells,
        const o_vector<Spring>& all_springs,
        const o_vector<Body>& all_bodies);

    bool is_spring_already_found(const Spring* spring) const;
    bool is_cell_already_found(const Cell* cell) const;
    const Spring* find_if_spring_connects_to_cell(const Cell* cell, const o_vector<Spring>& all_springs) const;

    void update_statistics(const SimpleSpatialGrid* /*food_grid*/, const SimpleSpatialGrid* /*cell_grid*/,
        const o_vector<Food>& /*food_vector*/, const RenderData& /*render_data*/)
    {
        // update_neighbourhood_stats(food_grid, cell_grid, food_vector, render_data, ...);
    }

private:
    void deactivate();
    void collect_connected(const o_vector<Cell>& all_cells,
        const o_vector<Spring>& all_springs,
        const o_vector<Body>& all_bodies);
    void update_bounds();
    void update_locomotion_stats(bool changed);
    void update_neighbourhood_stats(const SimpleSpatialGrid* food_grid, const SimpleSpatialGrid* cell_grid,
        const o_vector<Food>& food_vector, const RenderData& render_data, const o_vector<Body>& all_bodies);
    void update_energy();
};