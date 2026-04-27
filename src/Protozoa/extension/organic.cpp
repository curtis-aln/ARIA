#include "../Protozoa.h"
#include "../../Food/food_manager.h"
#include "../genetics/CellGenome.h"

#include <vector>


//void Protozoa::record_nearby_food(Food* food)
//{
    //food_positions_nearby.push_back(food->position);
//}

void Protozoa::consume(Food* food, FoodManager& food_manager)
{
    food_manager.remove_food(food->id);
    total_food_eaten++;
    stomach++;
    energy += food->nutrients;
}

void Protozoa::reproduce_check()
{
    if (time_since_last_reproduced++ < reproductive_cooldown_calculator()) // todo
        return;

	if (stomach < stomach_reproduce_thresh())
        return;

    time_since_last_reproduced = 0.f;
    stomach = 0;
    reproduce = true;
    offspring_count++;
}


void Protozoa::update_generation()
{
    for (Cell& cell : m_cells_)
    {
        cell.generation++;
    }
    for (Spring& spring : m_springs_)
    {
        spring.generation++;
	}
}

bool Protozoa::cell_wander_check(Cell& cell)
{
    const sf::Vector2f center = get_center();
    const float distance_sq = (cell.position_ - center).lengthSquared();
    
	return distance_sq > wander_threshold * wander_threshold;
}

void Protozoa::create_offspring(Protozoa* parent, bool should_mutate)
{
    // This protozoa should have been just created by the parent
    parent->reproduce = false;

    // first we assign the genetic aspects of the offspring to match that of the parents, then reconstruct it
    soft_reset();
    set_protozoa_attributes(parent);

    // incrementing the generation in all of the cells and springs
    update_generation();

    if (should_mutate)
        mutate();
    birth_location = parent->get_center();

    // we offset the offspring's position slightly from the parent as if it spawns directly in its parent
    // it can cause a sudden push on eachovers cells which could result in spring breaking and cell death
    float parent_bounds_x = parent->get_bounds().size.x;
    float parent_bounds_y = parent->get_bounds().size.y;
    float disp_x = Random::rand_range(-parent_bounds_x, parent_bounds_x);
    float disp_y = Random::rand_range(-parent_bounds_y, parent_bounds_y);
    move_center_location_to(parent->get_center() + sf::Vector2f{ disp_x, disp_y });
}

void Protozoa::kill()
{
    if (!immortal)
        dead = true;
}

void Protozoa::force_reproduce()
{
    reproduce = true;
}

void Protozoa::inject(const float energy_injected)
{
    energy += energy_injected;
}