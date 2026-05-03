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

    offspring->set_protozoa_attributes(this);
    offspring->soft_reset();

    // for each cell in the offspring we adjust its size slightly so it becomes comloser to their average size
	float avg = 0.f;
	for (Cell& cell : offspring->m_cells_)
	{
		avg += cell.radius;
	}
	avg /= offspring->m_cells_.size();
    const float radius_converge_constant = 0.1f;
	for (Cell& cell : offspring->m_cells_)
	{
		cell.radius += (avg - cell.radius) * radius_converge_constant;
	}

	// we do the same for the inner and outer colors
	float inner_r, inner_g, inner_b, outer_r, outer_g, outer_b;
	for (Cell& cell : offspring->m_cells_)
	{
		inner_r += cell.inner_r;
		inner_g += cell.inner_g;
		inner_b += cell.inner_b;
		outer_r += cell.outer_r;
		outer_g += cell.outer_g;
		outer_b += cell.outer_b;
	}
	inner_r /= offspring->m_cells_.size();
	inner_g /= offspring->m_cells_.size();
	inner_b /= offspring->m_cells_.size();
	outer_r /= offspring->m_cells_.size();
	outer_g /= offspring->m_cells_.size();
	outer_b /= offspring->m_cells_.size();

	const float color_converge_constant = 0.1f;

	for (Cell& cell : offspring->m_cells_)
	{
		cell.inner_r += (inner_r - cell.inner_r) * color_converge_constant;
		cell.inner_g += (inner_g - cell.inner_g) * color_converge_constant;
		cell.inner_b += (inner_b - cell.inner_b) * color_converge_constant;
		cell.outer_r += (outer_r - cell.outer_r) * color_converge_constant;
		cell.outer_g += (outer_g - cell.outer_g) * color_converge_constant;
		cell.outer_b += (outer_b - cell.outer_b) * color_converge_constant;
	}

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
		cell.time_since_last_reproduced_ = 0;
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