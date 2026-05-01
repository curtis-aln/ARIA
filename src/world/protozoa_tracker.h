#pragma once 

#include "../Protozoa/Protozoa.h"
#include "../settings.h"
#include "world_state.h"

#include "../Utils/Graphics/spatial_grid/simple_spatial_grid.h"
#include "Food/Food.h"
#include "Utils/o_vector.hpp"

inline static constexpr int neighbours_max = 100;
inline static FixedSpan<obj_idx> container{ neighbours_max };

// a class dedicated to tracking statistics about a protozoa
class ProtozoaTracker
{

public:
    int id{};

    sf::FloatRect bounds{};

    sf::Vector2f position{};
    sf::Vector2f position_prev{};

    sf::Vector2f velocity{};
    sf::Vector2f velocity_prev{};

    std::vector<Cell> cells{};
    std::vector<Spring> springs{};

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
    float ready_to_reproduce_percentage = 0;

    int total_food_eaten = 0;

    bool immortal = false;

    ProtozoaTracker() = default;


    void update_statistics(const Protozoa* protozoa_ptr, const SimpleSpatialGrid* food_grid, const SimpleSpatialGrid* cell_grid, const o_vector<Food>& food_vector, const RenderData& render_data)
    {
        if (protozoa_ptr == nullptr || !protozoa_ptr->is_alive())
        {
            return;
        }

        bool changed = protozoa_ptr->id != id;

        update_locomotion_stats(protozoa_ptr, changed);
        update_neighbourhood_stats(food_grid, cell_grid, food_vector, render_data);
        update_energy(protozoa_ptr);
        update_misc(protozoa_ptr);
    }

private:
    void update_locomotion_stats(const Protozoa* protozoa_ptr, const bool changed)
    {
        bounds = Protozoa::calc_protozoa_bounds(protozoa_ptr);
        position_prev = position;
        position = bounds.getCenter();

        if (changed)
        {
            id = protozoa_ptr->id;
            position_prev = position;
            velocity_prev = { 0, 0 };
        }

        velocity_prev = velocity;
        velocity = position - position_prev;
        acceleration = velocity - velocity_prev;

        speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
    }


    void update_neighbourhood_stats(const SimpleSpatialGrid* food_grid, const SimpleSpatialGrid* cell_grid, const o_vector<Food>& food_vector, const RenderData& render_data)
    {
        food_in_neighbourhood = 0;
        cells_in_neighbourhood = 0;


        cell_idx index = food_grid->hash(position.x, position.y);
        food_grid->find_from_index(index, &container);

        for (uint8_t i = 0; i < container.count; ++i)
        {
            const obj_idx food_id = container[i];
            const Food* food = food_vector.at(food_id);
            if (food != nullptr)
            {
                nearby_food_positions[food_in_neighbourhood++] = food->position;
            }
        }

        cell_grid->find_from_index(index, &container);
        for (uint8_t i = 0; i < container.count; ++i)
        {
            const obj_idx cell_id = container[i];
            const sf::Vector2f cell_position = { render_data.positions_x[cell_id], render_data.positions_y[cell_id] };
            nearby_cell_positions[cells_in_neighbourhood++] = cell_position;
        }

        total_in_neighbourhood = cells_in_neighbourhood + food_in_neighbourhood;
    }

    void update_energy(const Protozoa* protozoa_ptr)
    {
        cells = protozoa_ptr->get_cells();
        cell_count = static_cast<int>(cells.size());

        total_energy = 0;
        total_nutrients = 0;
        frames_alive = 0;
        total_food_eaten = 0;
		average_generation = 0;
        for (const Cell& cell : cells)
        {
            total_energy += cell.energy;
            total_nutrients += cell.nutrients_;
            frames_alive += cell.frames_alive_;
            total_food_eaten += cell.total_food_eaten_;
			average_generation += cell.generation;

            cells_ready_to_reproduce += cell.can_reproduce();
        }

        average_generation /= cell_count;
        max_energy = cell_count * ProtozoaSettings::reproduce_energy_thresh;
        max_nutrients = max_energy;
        average_energy = total_energy / cell_count;
        frames_alive = frames_alive / cell_count;
        ready_to_reproduce_percentage = (cells_ready_to_reproduce / cell_count) * 100;

        springs = protozoa_ptr->get_springs();
        spring_count = static_cast<int>(springs.size());

        spring_total_work_done = 0.f;
        for (Spring& spring : springs)
        {
            spring_total_work_done += spring.work_done;
        }
    }

    void update_misc(const Protozoa* protozoa_ptr)
    {
        offspring_count = protozoa_ptr->offspring_count;
        time_since_last_reproduced = protozoa_ptr->time_since_last_reproduced;
        reproduction_cooldown = ProtozoaSettings::reproductive_cooldown;
        immortal = protozoa_ptr->immortal;
    }
};