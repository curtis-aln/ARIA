#include "../Protozoa.h"
#include "../genetics/CellGenome.h"

#include <vector>

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
    birth_location = parent->get_cells()[0].position_;
	birth_location += sf::Vector2f{ Random::rand_range(-spawn_radius, spawn_radius), Random::rand_range(-spawn_radius, spawn_radius) };

    move_center_location_to(birth_location);
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