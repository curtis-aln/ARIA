#include "../Protozoa.h"
#include "../genetics/CellGenome.h"

#include <vector>



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


void Protozoa::inject(const float energy_injected)
{
	const auto cell_count = static_cast<float>(m_cells_.size());
    for (Cell& cell : m_cells_)
    {
        cell.energy += energy_injected / cell_count;
    }
}