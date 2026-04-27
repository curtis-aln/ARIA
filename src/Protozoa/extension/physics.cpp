#include "../Protozoa.h"

#include "../../Food/food_manager.h"

Protozoa::Protozoa(const int id_)
	: id(id_)
{

}


void Protozoa::update()
{
	if (m_cells_.empty()) // No computation is needed if there are no cells
		return;

	check_death_conditions();

	update_springs();

	update_cells();

	reproduce_check();

	++frames_alive;

	energy -= energy_decay_rate;

}


void Protozoa::update_springs()
{
	// updates the springs connecting the cells
	energy_lost_to_springs = 0.f;
	for (Spring& spring : m_springs_)
	{
		Cell& cell_A = m_cells_[spring.cell_A_id];
		Cell& cell_B = m_cells_[spring.cell_B_id];
		SpringResult result = spring.update(cell_A, cell_B, frames_alive);
		energy -= result.work_done;
		energy_lost_to_springs += result.work_done;

		if (result.broken)
		{
			kill();
			return;
		}
	}
}

void Protozoa::update_cells()
{
	// updates each cell in the organism
	for (Cell& cell : m_cells_)
	{
		cell.update(frames_alive);

		energy += cell.nutrients_eaten;
		cell.nutrients_eaten = 0.f;
		stomach += cell.food_eaten;
		cell.food_eaten = 0;
	}
}

void Protozoa::move_center_location_to(const sf::Vector2f new_center)
{
	const sf::Vector2f center = m_cells_[0].position_;
	const sf::Vector2f translation = new_center - center;
	for (Cell& cell : m_cells_)
	{
		cell.position_ += translation;
	}
}


void Protozoa::soft_reset()
{
	frames_alive = 0.f;
	dead = false;
	reproduce = false;

	stomach = 0;
	total_food_eaten = 0;
	offspring_count = 0;
	energy = initial_energy;
	immortal = false;

	time_since_last_reproduced = 0;

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

	birth_location = { 0, 0 };

	active = true; // for o_vector.h
}



Protozoa::Protozoa(const Protozoa& other)
	: ProtozoaSettings()
	, m_cells_(other.m_cells_)
	, m_springs_(other.m_springs_)
	, id(other.id)
	, active(other.active)
	, time_since_last_reproduced(other.time_since_last_reproduced)
	, birth_location(other.birth_location)
	, energy_lost_to_springs(other.energy_lost_to_springs)
	, offspring_count(other.offspring_count)
	, frames_alive(other.frames_alive)
	, total_food_eaten(other.total_food_eaten)
	, immortal(other.immortal)
	, stomach(other.stomach)
	, energy(other.energy)
	, reproduce(other.reproduce)
	, dead(other.dead)
{}

Protozoa& Protozoa::operator=(const Protozoa& other)
{
	if (this == &other) return *this;

	m_cells_ = other.m_cells_;
	m_springs_ = other.m_springs_;
	id = other.id;
	active = other.active;
	time_since_last_reproduced = other.time_since_last_reproduced;
	birth_location = other.birth_location;
	energy_lost_to_springs = other.energy_lost_to_springs;
	offspring_count = other.offspring_count;
	frames_alive = other.frames_alive;
	total_food_eaten = other.total_food_eaten;
	immortal = other.immortal;
	stomach = other.stomach;
	energy = other.energy;
	reproduce = other.reproduce;
	dead = other.dead;
	// m_window_ and m_world_bounds_ intentionally not copied

	return *this;
}