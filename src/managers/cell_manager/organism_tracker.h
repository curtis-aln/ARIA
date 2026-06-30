#pragma once 

#include <array>

#include "../../world/world_state.h"

#include "../../Utils/spatial_grid/simple_spatial_grid.h"
#include "../../entities/food/food.h"
#include "../../entities/cell/cell.h"
#include "../../entities/spring/spring.h"
#include "Utils/o_vec/o_vector.hpp"

inline static constexpr int neighbours_max = 100;
inline static FixedSpan<obj_idx> container{ neighbours_max };


// a class dedicated to tracking statistics about a protozoa
// it does this by using a starting cell which the user would have selected, and then it uses that cell and the 
// spring container to find all the other cells and springs that are connected to it, and then it uses those to calculate statistics about the protozoa

class OrganismTracker
{

public:
    int selected_id = -1;
    bool is_active = false;

    sf::FloatRect bounds{};

    sf::Vector2f position{};
    sf::Vector2f position_prev{};

    sf::Vector2f velocity{};
    sf::Vector2f velocity_prev{};

    std::vector<Cell> cells{};
    std::vector<Spring> springs{};
    std::vector<Body> bodies{};

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

    OrganismTracker() = default;


    // The new and improved update function now that we dont have a global protozoa class
    void update_primitive(const Cell* selected_cell, const o_vector<Cell>& all_cells, const o_vector<Spring>& all_springs, const o_vector<Body>& all_bodies)
    {
		if (selected_cell == nullptr)
		{
			is_active = false;
			return;
		}

        // detecting if the selected protozoa has changed to recalculate some stats
        bool changed = false;
        if (selected_id != selected_cell->id_)
        {
            selected_id = selected_cell->id_;
            changed = true;
        }

		const Body* selected_body = all_bodies.at(selected_cell->body_id_);
    
        cells.clear();
        springs.clear();
        bodies.clear();

		cells.push_back(*selected_cell);
        bodies.push_back(*selected_body);

        find_next_cell(selected_cell, all_cells, all_springs, all_bodies);

        update_bounds(all_bodies);

        update_locomotion_stats(changed);
        update_energy();
    }

    void find_next_cell(const Cell* parent_cell,
        const o_vector<Cell>& all_cells,
        const o_vector<Spring>& all_springs, const o_vector<Body>& all_bodies)
    {
        // Iterate ALL springs — a cell can have multiple connections
        for (const Spring* spring : all_springs)
        {
            // Skip springs that don't touch this cell
            if (spring->cell_A_id != parent_cell->id_ &&
                spring->cell_B_id != parent_cell->id_)
                continue;

            // Skip springs already traversed — prevents duplicate entries
            // and avoids re-walking already-visited branches
            if (is_spring_already_found(spring))
                continue;

            springs.push_back(*spring);

            // The neighbour is whichever end of the spring isn't the parent
            const uint32_t neighbour_id = (spring->cell_A_id == parent_cell->id_)
                ? spring->cell_B_id
                : spring->cell_A_id;

            const Cell* next_cell = all_cells.at(neighbour_id);
            if (next_cell != nullptr && !is_cell_already_found(next_cell))
            {
                cells.push_back(*next_cell);
                bodies.push_back(*all_bodies.at(next_cell->body_id_));
                find_next_cell(next_cell, all_cells, all_springs, all_bodies);
            }
        }
    }

    bool is_spring_already_found(const Spring* spring) const
    {
        for (const Spring& s : springs)
        {
            if (s.id_ == spring->id_)
                return true;
        }
        return false;
    }

	bool is_cell_already_found(const Cell* cell) const
	{
		for (const Cell& c : cells)
		{
			if (c.id_ == cell->id_)
				return true;
		}
		return false;
	}

    const Spring* find_if_spring_connects_to_cell(const Cell* cell, const o_vector<Spring>& all_springs) const
    {
        int id = cell->id_;
        for (const Spring* spring : all_springs)
        {
            if (spring->cell_A_id == id || spring->cell_B_id == id)
            {
                return spring;
            }
        }
        return nullptr;
    }

	void update_bounds(const o_vector<Body>& all_bodies)
	{
		if (cells.empty())
		{
			bounds = {};
			return;
		}
		float min_x = std::numeric_limits<float>::max();
		float min_y = std::numeric_limits<float>::max();
		float max_x = std::numeric_limits<float>::lowest();
		float max_y = std::numeric_limits<float>::lowest();

		for (const Cell& cell : cells)
		{
			const Body* body = all_bodies.at(cell.body_id_);
			if (body != nullptr)
			{
				const sf::Vector2f& pos = body->position_;
				const float rad = body->radius_;
				min_x = std::min(min_x, pos.x - rad);
				min_y = std::min(min_y, pos.y - rad);
				max_x = std::max(max_x, pos.x + rad);
				max_y = std::max(max_y, pos.y + rad);
			}
		}

		bounds.position.x = min_x;
		bounds.position.y = min_y;
		bounds.size.x = max_x - min_x;
		bounds.size.y = max_y - min_y;
		position_prev = position;
		position.x = bounds.position.x + bounds.size.x / 2.f;
		position.y = bounds.position.y + bounds.size.y / 2.f;
		velocity_prev = velocity;
		velocity = position - position_prev;
	}

    void update_statistics(const SimpleSpatialGrid* food_grid, const SimpleSpatialGrid* cell_grid, const o_vector<Food>& food_vector, const RenderData& render_data)
    {
        // update_neighbourhood_stats(food_grid, cell_grid, food_vector, render_data);
        // update_misc(protozoa_ptr)
    }

private:
    void update_locomotion_stats(const bool changed)
    {
        position_prev = position;
        position = bounds.getCenter();

        if (changed)
        {
            position_prev = position;
            velocity_prev = { 0, 0 };
        }

        velocity_prev = velocity;
        velocity = position - position_prev;
        acceleration = velocity - velocity_prev;

        speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
		 
    }


    void update_neighbourhood_stats(const SimpleSpatialGrid* food_grid, const SimpleSpatialGrid* cell_grid, const o_vector<Food>& food_vector, const RenderData& render_data, const o_vector<Body>& all_bodies)
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
                // todo
				//const Body* body = all_bodies->at(food->body_id_);
                //nearby_food_positions[food_in_neighbourhood++] = body->position_;
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

    void update_energy()
    {
        cell_count = static_cast<int>(cells.size());

        total_energy = 0;
        total_nutrients = 0;
        frames_alive = 0;
        total_food_eaten = 0;
		average_generation = 0;
        offspring_count = 0;
		time_since_last_reproduced = 0;
        for (const Cell& cell : cells)
        {
            total_energy += cell.energy;
            total_nutrients += cell.nutrients_;
            frames_alive += cell.frames_alive_;
            total_food_eaten += cell.total_food_eaten_;
			average_generation += cell.generation;
            offspring_count += cell.offspring_count;
			time_since_last_reproduced += cell.time_since_last_reproduced_;

            cells_ready_to_reproduce += cell.can_reproduce();
        }

        average_generation /= cell_count;
        max_energy = cell_count * CellSettings::reproduce_energy_thresh;
        max_nutrients = max_energy;
        average_energy = total_energy / cell_count;
        frames_alive = frames_alive / cell_count;
        ready_to_reproduce_percentage = (cells_ready_to_reproduce / cell_count) * 100;
		offspring_count = offspring_count / cell_count;
		time_since_last_reproduced = time_since_last_reproduced / cell_count;

        spring_count = static_cast<int>(springs.size());

        spring_total_work_done = 0.f;
        for (Spring& spring : springs)
        {
            spring_total_work_done += spring.work_done;
        }
		
    }

    void update_misc()
    {
        //reproduction_cooldown = ProtozoaSettings::reproductive_cooldown;
    }
};