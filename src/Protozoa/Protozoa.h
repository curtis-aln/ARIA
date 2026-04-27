#pragma once
#include <SFML/System/Vector2.hpp>

#include "../settings.h"
#include "cell.h"
#include "spring.h"


#include "../Food/food_manager.h"

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
	uint16_t frames_alive = 0;
	uint8_t total_food_eaten = 0;

private:
	// components of the protozoa
	alignas (64) std::vector<Cell> m_cells_{};
	alignas (64) std::vector<Spring> m_springs_{};	


	uint8_t stomach = 0;
	float energy = initial_energy;

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
	[[nodiscard]] float get_energy() const { return energy; }
	[[nodiscard]] unsigned stomach_capacity() const { return stomach; }
	[[nodiscard]] size_t stomach_reproduce_thresh() const { return m_cells_.size(); }
	[[nodiscard]] size_t reproductive_cooldown_calculator() const { return reproductive_cooldown / m_cells_.size(); }
	[[nodiscard]] int get_cell_count() const { return static_cast<int>(m_cells_.size()); }
	[[nodiscard]] int get_spring_count() const { return static_cast<int>(m_springs_.size()); }


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
	// updating
	void update_springs();
	void update_cells();
	void update_generation();

	// organic
	void reproduce_check();
	void consume(const Food* food, FoodManager& food_manager);
	

	// mutating
	void mutate_existing_cells(float mut_rate = 0.f, float mut_range = 0.f);
	void mutate_existing_springs(float mut_rate = 0.f, float mut_range = 0.f);


	void check_death_conditions()
	{
		if (energy <= 0)
			kill();
	}

};
