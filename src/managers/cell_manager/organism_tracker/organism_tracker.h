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
#include "../../../entities/spring/tendon.h"
#include "Utils/o_vec/o_vector.hpp"

inline constexpr int neighbours_max = 100;


// A class dedicated to tracking statistics about a protozoa (an organism).
// It starts from the cell the user selected, walks the tendon container to find every cell and
// tendon connected to it, and uses those to calculate statistics about the whole organism.
//
// Energy and nutrient capacities are per cell (they depend on each cell's mass), so the organism's
// max_energy / max_nutrients are the SUM of its cells' individual maxes, not a global constant.
// That keeps total_energy / max_energy a valid fill fraction for the organism as a whole.
//
// organism_tracker.cpp holds all the implementations.
class OrganismTracker
{
public:
    // Pass this to update_primitive() to mean "nothing is selected".
    static constexpr uint32_t invalid_cell_id = std::numeric_limits<uint32_t>::max();

    // ---- selection ------------------------------------------------------------------------
    bool is_active = false;
    int  selected_id = -1;
    int  original_selected_cell_id = -1;

    // ---- organism contents ----------------------------------------------------------------
    std::vector<Cell>   cells{};
    std::vector<Tendon> springs{};
    std::vector<Body>   bodies{};  // bodies[i] belongs to cells[i]

    int cell_count = 0;
    int tendon_count = 0;

    // ---- geometry and locomotion ----------------------------------------------------------
    sf::FloatRect bounds{};

    sf::Vector2f position{};
    sf::Vector2f position_prev{};
    sf::Vector2f velocity{};
    sf::Vector2f velocity_prev{};
    sf::Vector2f acceleration{};
    float speed{};

    // ---- neighbourhood --------------------------------------------------------------------
    std::array<sf::Vector2f, neighbours_max> nearby_food_positions{};
    std::array<sf::Vector2f, neighbours_max> nearby_cell_positions{};
    int cells_in_neighbourhood = 0;
    int food_in_neighbourhood = 0;
    int total_in_neighbourhood = 0;

    // ---- energy ---------------------------------------------------------------------------
    // max_* are sums of each cell's own capacity.
    float total_energy = 0.f;
    float average_energy = 0.f;   // total_energy / cell_count
    float max_energy = 0.f;
    float spring_total_work_done = 0.f;

    // ---- nutrients ------------------------------------------------------------------------
    float total_nutrients = 0.f;
    float max_nutrients = 0.f;

    // ---- life history ---------------------------------------------------------------------
    float average_generation = 0.f;
    float frames_alive = 0.f;
    int   total_food_eaten = 0;

    // ---- reproduction ---------------------------------------------------------------------
    int   offspring_count = 0;
    int   time_since_last_reproduced = 0;
    int   reproduction_cooldown = 0;
    int   cells_ready_to_reproduce = 0;
    float ready_to_reproduce_percentage = 0.f;

    OrganismTracker() = default;

    // ---- public API -----------------------------------------------------------------------

    // Rebuilds the organism from the selected cell and refreshes every statistic.
    void update_primitive(uint32_t selected_cell_id,
        const o_vector<Cell>& all_cells,
        const o_vector<Tendon>& all_springs,
        const o_vector<Body>& all_bodies);

    void update_statistics(const SimpleSpatialGrid* /*food_grid*/, const SimpleSpatialGrid* /*cell_grid*/,
        const o_vector<Food>& /*food_vector*/, const RenderData& /*render_data*/)
    {
        // update_neighbourhood_stats(food_grid, cell_grid, food_vector, render_data, ...);
    }

    // Fill fractions in [0, 1] for the whole organism. Safe when the max is zero (empty organism).
    float energy_fraction() const { return max_energy > 0.f ? total_energy / max_energy : 0.f; }
    float nutrients_fraction() const { return max_nutrients > 0.f ? total_nutrients / max_nutrients : 0.f; }

    // ---- lookups --------------------------------------------------------------------------
    bool is_tendon_already_found(const Tendon* tendon) const;
    bool is_cell_already_found(const Cell* cell) const;
    const Tendon* find_if_tendon_connects_to_cell(const Cell* cell, const o_vector<Tendon>& all_tendons) const;

private:
    void deactivate();

    void collect_connected(const o_vector<Cell>& all_cells,
        const o_vector<Tendon>& all_springs,
        const o_vector<Body>& all_bodies);

    void update_bounds();
    void update_locomotion_stats(bool changed);
    void update_energy();
    void update_neighbourhood_stats(const SimpleSpatialGrid* food_grid, const SimpleSpatialGrid* cell_grid,
        const o_vector<Food>& food_vector, const RenderData& render_data, const o_vector<Body>& all_bodies);
};