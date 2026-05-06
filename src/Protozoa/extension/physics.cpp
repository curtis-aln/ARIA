#include "../Protozoa.h"

Protozoa::Protozoa(const int id_)
	: id(id_)
{

}


void Protozoa::update()
{
	if (m_cells_.empty()) // No computation is needed if there are no cells
		return;

	update_springs();

	update_cells();
}


void Protozoa::update_springs()
{
	// updates the springs connecting the cells
	for (Spring& spring : m_springs_)
	{
		Cell& cell_A = m_cells_[spring.cell_A_id];
		Cell& cell_B = m_cells_[spring.cell_B_id];
		spring.update(cell_A, cell_B);

		cell_A.energy -= spring.work_done / 2.f; // Eventually springs will be their own organic systems with energy
		cell_B.energy -= spring.work_done / 2.f;


		if (spring.stress > ProtozoaSettings::spring_damage_threshold)
		{
			float excess = spring.stress - ProtozoaSettings::spring_damage_threshold;
			//cell_A.integrity -= excess;
			//cell_B.integrity -= excess;
		}

		if (spring.broken)
		{
			remove_spring(spring.id);
		}
	}
}

void Protozoa::update_cells()
{
	// updates each cell in the organism
	for (Cell& cell : m_cells_)
	{
		cell.update();
	}
}

void Protozoa::move_center_location_to(const sf::Vector2f new_center)
{
	const sf::Vector2f center = m_cells_[0].get_pos();
	const sf::Vector2f translation = new_center - center;
	for (Cell& cell : m_cells_)
	{
		const sf::Vector2f& current_pos = cell.get_pos();
		const sf::Vector2f new_pos = current_pos + translation;
		cell.set_pos(new_pos);
	}
}


void Protozoa::soft_reset()
{
	for (Cell& cell : m_cells_)
	{
		cell.reset();
	}
}

void Protozoa::hard_reset()
{
	soft_reset();

	m_cells_.clear();
	m_springs_.clear();


	active = true; // for o_vector.h
}



Protozoa::Protozoa(const Protozoa& other)
	: ProtozoaSettings()
	, m_cells_(other.m_cells_)
	, m_springs_(other.m_springs_)
	, id(other.id)
	, active(other.active)
{}

Protozoa& Protozoa::operator=(const Protozoa& other)
{
	if (this == &other) return *this;

	m_cells_ = other.m_cells_;
	m_springs_ = other.m_springs_;
	id = other.id;
	active = other.active;
	// m_window_ and m_world_bounds_ intentionally not copied

	return *this;
}