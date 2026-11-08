#include "organism_tracker.h"

#include <algorithm>
#include <cmath>

// ---- public entry point --------------------------------------------------------------------

void OrganismTracker::update_primitive(const uint32_t selected_cell_id,
    const o_vector<Cell>& all_cells,
    const o_vector<Spring>& all_springs,
    const o_vector<Body>& all_bodies)
{
    if (selected_cell_id == invalid_cell_id)
    {
        deactivate();
        return;
    }

    original_selected_cell_id = static_cast<int>(selected_cell_id);

    const Cell* selected_cell = all_cells.at(selected_cell_id);
    const Body* selected_body = selected_cell ? all_bodies.at(selected_cell->body_id_) : nullptr;
    if (selected_cell == nullptr || selected_body == nullptr)
    {
        deactivate();
        return;
    }

    // Detect a changed selection so velocity isn't computed against the previous organism.
    const bool changed = (selected_id != static_cast<int>(selected_cell->id_));
    selected_id = static_cast<int>(selected_cell->id_);

    cells.clear();
    springs.clear();
    bodies.clear();

    cells.push_back(*selected_cell);
    bodies.push_back(*selected_body);

    collect_connected(all_cells, all_springs, all_bodies);

    update_bounds();
    update_locomotion_stats(changed);
    update_energy();

    is_active = true;
}

void OrganismTracker::deactivate()
{
    is_active = false;
    selected_id = -1; // forces "changed" on the next selection so velocity resets
    cells.clear();
    springs.clear();
    bodies.clear();
    bounds = {};
}

// ---- organism discovery --------------------------------------------------------------------

// Expands outward from whatever is already in `cells` until no new cell is found.
// Each pass scans the springs once; a spring touching any found cell belongs to the organism.
// This replaces the old recursion, which rescanned every spring in the world once per cell.
void OrganismTracker::collect_connected(const o_vector<Cell>& all_cells,
    const o_vector<Spring>& all_springs,
    const o_vector<Body>& all_bodies)
{
    const auto has_cell = [this](const auto cell_id)
        {
            return std::any_of(cells.begin(), cells.end(),
                [cell_id](const Cell& c) { return c.id_ == cell_id; });
        };

    bool grew = true;
    while (grew)
    {
        grew = false;

        for (const Spring* spring : all_springs)
        {
            const bool a_found = has_cell(spring->cell_A_id);
            const bool b_found = has_cell(spring->cell_B_id);

            if (!a_found && !b_found)
                continue; // unrelated to this organism (so far)

            if (is_spring_already_found(spring))
                continue;

            springs.push_back(*spring);

            if (a_found && b_found)
                continue; // closes a loop, nothing new to add

            const uint32_t neighbour_id = a_found ? spring->cell_B_id : spring->cell_A_id;

            const Cell* neighbour = all_cells.at(neighbour_id);
            if (neighbour == nullptr)
                continue;

            const Body* neighbour_body = all_bodies.at(neighbour->body_id_);
            if (neighbour_body == nullptr)
                continue; // keep cells[] and bodies[] aligned

            cells.push_back(*neighbour);
            bodies.push_back(*neighbour_body);
            grew = true;
        }
    }
}

bool OrganismTracker::is_spring_already_found(const Spring* spring) const
{
    return std::any_of(springs.begin(), springs.end(),
        [spring](const Spring& s) { return s.id_ == spring->id_; });
}

bool OrganismTracker::is_cell_already_found(const Cell* cell) const
{
    return std::any_of(cells.begin(), cells.end(),
        [cell](const Cell& c) { return c.id_ == cell->id_; });
}

const Spring* OrganismTracker::find_if_spring_connects_to_cell(const Cell* cell, const o_vector<Spring>& all_springs) const
{
    if (cell == nullptr)
        return nullptr;

    const auto id = cell->id_;
    for (const Spring* spring : all_springs)
    {
        if (spring->cell_A_id == id || spring->cell_B_id == id)
            return spring;
    }
    return nullptr;
}

// ---- statistics ----------------------------------------------------------------------------

void OrganismTracker::update_bounds()
{
    if (bodies.empty())
    {
        bounds = {};
        return;
    }

    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_y = std::numeric_limits<float>::lowest();

    for (const Body& body : bodies)
    {
        const sf::Vector2f pos = body.get_pos();
        const float rad = body.get_radius();
        min_x = std::min(min_x, pos.x - rad);
        min_y = std::min(min_y, pos.y - rad);
        max_x = std::max(max_x, pos.x + rad);
        max_y = std::max(max_y, pos.y + rad);
    }

    bounds.position = { min_x, min_y };
    bounds.size = { max_x - min_x, max_y - min_y };
}

// The only place position/velocity/acceleration are updated.
void OrganismTracker::update_locomotion_stats(const bool changed)
{
    const sf::Vector2f new_position = bounds.getCenter();

    if (changed)
    {
        // Different organism: the old position is no use as a reference, so start from rest.
        position = new_position;
        position_prev = new_position;
        velocity = {};
        velocity_prev = {};
    }
    else
    {
        position_prev = position;
        position = new_position;
        velocity_prev = velocity;
        velocity = position - position_prev;
    }

    acceleration = velocity - velocity_prev;
    speed = velocity.length();
}

void OrganismTracker::update_energy()
{
    cell_count = static_cast<int>(cells.size());
    spring_count = static_cast<int>(springs.size());

    total_energy = 0.f;
    total_nutrients = 0.f;
    frames_alive = 0.f;
    total_food_eaten = 0;
    average_generation = 0.f;
    offspring_count = 0;
    time_since_last_reproduced = 0;
    cells_ready_to_reproduce = 0; // was never reset before, so it grew every frame
    spring_total_work_done = 0.f;

    for (const Cell& cell : cells)
    {
        total_energy += cell.get_energy();
        total_nutrients += cell.nutrients_;
        frames_alive += cell.internal_clock_;
        total_food_eaten += cell.total_food_eaten_;
        average_generation += cell.get_generation();
        offspring_count += cell.offspring_count;
        time_since_last_reproduced += cell.repro_timer_;

        cells_ready_to_reproduce += cell.should_reproduce();
    }

    for (const Spring& spring : springs)
        spring_total_work_done += spring.work_done;

    if (cell_count == 0)
    {
        average_energy = max_energy = max_nutrients = ready_to_reproduce_percentage = 0.f;
        return;
    }

    const float count = static_cast<float>(cell_count);

    average_generation /= count;
    max_energy = count * CellSettings::max_energy;
    max_nutrients = max_energy;
    average_energy = total_energy / count;
    frames_alive /= count;
    ready_to_reproduce_percentage = 100.f * static_cast<float>(cells_ready_to_reproduce) / count; // was integer division

    offspring_count /= cell_count;
    time_since_last_reproduced /= cell_count;
}

// Currently not called anywhere (update_statistics has its call commented out).
void OrganismTracker::update_neighbourhood_stats(const SimpleSpatialGrid* food_grid, const SimpleSpatialGrid* cell_grid,
    const o_vector<Food>& food_vector, const RenderData& render_data, [[maybe_unused]] const o_vector<Body>& all_bodies)
{
    // Scratch buffer for grid queries. Function-local and thread_local, so there is exactly one
    // per thread instead of one per translation unit.
    static thread_local FixedSpan<obj_idx> container{ neighbours_max };

    food_in_neighbourhood = 0;
    cells_in_neighbourhood = 0;

    // Each grid has its own cell size, so each needs its own hash.
    const cell_idx food_index = food_grid->hash(position.x, position.y);
    food_grid->find_from_index(food_index, &container);

    for (std::size_t i = 0; i < static_cast<std::size_t>(container.count); ++i)
    {
        const obj_idx food_id = container[i];
        const Food* food = food_vector.at(food_id);
        if (food != nullptr)
        {
            // todo
            //const Body* body = all_bodies.at(food->body_id_);
            //nearby_food_positions[food_in_neighbourhood++] = body->position_;
        }
    }

    const cell_idx cell_index = cell_grid->hash(position.x, position.y);
    cell_grid->find_from_index(cell_index, &container);

    for (std::size_t i = 0; i < static_cast<std::size_t>(container.count) && cells_in_neighbourhood < neighbours_max; ++i)
    {
        const obj_idx cell_id = container[i];
        nearby_cell_positions[cells_in_neighbourhood++] = render_data.positions[cell_id];
    }

    total_in_neighbourhood = cells_in_neighbourhood + food_in_neighbourhood;
}