#include "../Protozoa.h"
#include "../genetics/CellGenome.h"

#include <vector>

void Protozoa::reproduce_check()
{
	for (Cell& cell : m_cells_)
	{
		if (!cell.can_reproduce())
		{
            return;
		}
	}

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

void Protozoa::create_offspring(Protozoa* offspring, bool should_mutate)
{
    // This protozoa should have been just created by the parent
    reproduce = false;
    time_since_last_reproduced = 0.f;

    offspring->soft_reset();
    offspring->set_protozoa_attributes(this);

    // incrementing the generation in all of the cells and springs
    update_generation();

    if (should_mutate)
        mutate();
    birth_location = m_cells_[0].get_pos();
	birth_location += sf::Vector2f{ Random::rand_range(-spawn_radius, spawn_radius), Random::rand_range(-spawn_radius, spawn_radius) };

    move_center_location_to(birth_location);

	for (Cell& cell : m_cells_)
	{
        cell.energy -= ProtozoaSettings::offspring_energy_cost;
	}
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
	const auto cell_count = static_cast<float>(m_cells_.size());
    for (Cell& cell : m_cells_)
    {
        cell.energy += energy_injected / cell_count;
    }
}