#pragma once

#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Color.hpp>
#include <cmath>
#include <algorithm>

#include "cell_settings.h"
#include "cell_genome.h"

// Each organism consists of cells which work together via springs
// Each cell has their own radius and friction coefficient, as well as cosmetic factors such as color


struct Cell : public CellGenome, CellSettings
{
private:
	uint16_t clock_ = 0;

protected:
	sf::Vector2f position_{};
	sf::Vector2f velocity_{};

	bool dead = false;
	bool immortal = false;

public:
	// The Cell ID is used when referencing the cell inside the protozoa, and identifying its genome
	uint8_t id{}; 
	float energy = initial_energy;

	// Stomach and food
	uint16_t time_since_last_ate_ = 0;
	uint16_t time_since_last_reproduced_ = 0;
	float nutrients_ = 0.f;
	uint8_t total_food_eaten_ = 0;
	uint8_t stomach_ = 0;
	float integrity = integrity;

	float sinwave_current_friction_ = 0.f;

	bool active = true;

	// Statistics information
	uint16_t frames_alive_ = 0;
	uint8_t  offspring_count = 0;

	// reproductive related variables
	bool reproduce = false; // signals to the protozoa manager that this cell needs an offspring index set
	int8_t offspring_index = -1; // any value less than 0 means unfined
	int8_t connection_index = -1; // tells the protozoa manager what to connect offspring index to
	int8_t spring_to_copy_index = -1; // tells the protozoa manager which spring to copy 

	Cell(const uint32_t _id = 0, const sf::Vector2f position = {0, 0})
		: position_(position), id(_id)
	{
		
	}

	[[nodiscard]] bool is_alive() const { return !dead; }
	[[nodiscard]] bool should_reproduce() const { return reproduce; }

	[[nodiscard]] const sf::Vector2f& get_pos() const { return position_; }
	[[nodiscard]] sf::Vector2f& get_pos() { return position_; }
	[[nodiscard]] const sf::Vector2f& get_vel() const { return velocity_; }

	void set_pos(const sf::Vector2f& position) { position_ = position;}

	[[nodiscard]] sf::Color get_outer_color() const { return { outer_r, outer_g, outer_b, outer_transparency }; }
	[[nodiscard]] sf::Color get_inner_color() const { return { inner_r, inner_g, inner_b, inner_transparency }; }

	void reset();
	void eat(const float nutrients);

	static bool consume_food_check(const sf::Vector2f& cell_pos, const sf::Vector2f& food_pos, const float combined_rad);
	void update();

	void accelerate(const sf::Vector2f acceleration);

	void create_offspring(Cell* child, const bool mutate = true);

	[[nodiscard]] bool can_die() const;

	[[nodiscard]] bool can_reproduce() const;

	[[nodiscard]] sf::Vector2f get_pos_nearby(const float range) const;

	[[nodiscard]] float calculate_friction() const;

private:
	void update_statistics();


	void update_organics();
	void update_physics();

	void convert_nutrients_to_integrity();


	void convert_nutrients_to_energy();
};