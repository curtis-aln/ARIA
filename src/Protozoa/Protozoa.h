#pragma once
#include <SFML/System/Vector2.hpp>

#include "../settings.h"
#include "cell.h"
#include "spring.h"


// this is the organisms in the simulation, they are made up of cells which all act independently, attached by springs
// the protozoa class is responsible for connecting the springs and cells



// The Genome class handles anything that has to do with mutation and genetics
class Protozoa : ProtozoaSettings
{
public:
	int id = 0;
	bool active = true; // for o_vector.h

	// statistics informaiton
	uint16_t time_since_last_reproduced = 0;
	sf::Vector2f birth_location = { 0, 0 };

	float energy_lost_to_springs = 0.f;

	uint8_t offspring_count = 0;
	
	uint8_t total_food_eaten = 0;

private:
	// components of the protozoa
	alignas (64) std::vector<Cell> m_cells_{};
	alignas (64) std::vector<Spring> m_springs_{};	


	bool reproduce = false;
	bool dead = false;
	

	
public:
	bool immortal = false;

	Protozoa(int id_ = 0);
	Protozoa(const Protozoa& other);
	Protozoa& operator=(const Protozoa& other);

	
	void update();
	
	void mutate(const bool artificial_add_cell = false, const float artificial_mutatation_rate = 0.f, const float artificial_mutatation_range = 0.f);

	std::vector<Cell>& get_cells() { return m_cells_; }
	[[nodiscard]] const std::vector<Cell>& get_cells() const { return m_cells_; }
	std::vector<Spring>& get_springs() { return m_springs_; }
	[[nodiscard]] const std::vector<Spring>& get_springs() const { return m_springs_; }

	[[nodiscard]] bool is_alive() const { return !dead; }
	[[nodiscard]] bool should_reproduce() const { return reproduce; }
	[[nodiscard]] float get_energy() const
	{
		float total = 0;
		for (const Cell& cell : m_cells_)
		{
			total += cell.energy;
		}
		return total;
	}
	[[nodiscard]] unsigned stomach_capacity() const
	{
		int total = 0;
		for (const Cell& cell : m_cells_)
		{
			total += cell.stomach_;
		}
		return total;
	}
	[[nodiscard]] unsigned get_frames_alive_avg() const
	{
		unsigned sum = 0;
		unsigned count = m_cells_.size();

		for (const Cell& cell : m_cells_)
		{
			sum += cell.frames_alive_;
		}
		return count > 0 ? sum / count : 0;
	}

	[[nodiscard]] size_t stomach_reproduce_thresh() const { return m_cells_.size(); }
	[[nodiscard]] size_t reproductive_cooldown_calculator() const { return reproductive_cooldown / m_cells_.size(); }
	[[nodiscard]] int get_cell_count() const { return static_cast<int>(m_cells_.size()); }
	[[nodiscard]] int get_spring_count() const { return static_cast<int>(m_springs_.size()); }

	[[nodiscard]] int calc_frames_alive()
	{
		return m_cells_[0].frames_alive_; // todo
	}

	// information setting
	void move_center_location_to(const sf::Vector2f new_center);
	void create_offspring(Protozoa* parent, bool should_mutate = true);
	void force_reproduce();
	void inject(const float energy_injected);

	void set_protozoa_attributes(const Protozoa* other)
	{
		m_cells_ = other->m_cells_;
		m_springs_ = other->m_springs_;
	}

	void init_one_cell(sf::Vector2f& position)
	{
		m_cells_.clear();
		m_springs_.clear();

		m_cells_.emplace_back(0, position);
	}

	void kill();

	void soft_reset();
	void hard_reset();

	void add_spring();
	void add_cell();

	void remove_cell();
	void remove_spring();


private:
	void sync_clocks()
	{
		// Each cell has its own internal clock, we need to slowly sync them together
		const float sync_amount = 0.1f;
		for (Cell& cell : m_cells_)
		{
			
		}

	}

	// updating
	void update_springs();
	void update_cells();
	void update_generation();

	// organic
	void reproduce_check();
	

	// mutating
	void mutate_existing_cells(float mut_rate = 0.f, float mut_range = 0.f);
	void mutate_existing_springs(float mut_rate = 0.f, float mut_range = 0.f);


	void check_death_conditions()
	{
		for (Cell& cell : m_cells_)
		{
			if (cell.can_die())
			{
				kill();
			}
		}
	}

};
