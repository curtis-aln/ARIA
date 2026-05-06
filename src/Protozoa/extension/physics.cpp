#include "../Protozoa.h"

Protozoa::Protozoa(const int id_)
	: id(id_)
{

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