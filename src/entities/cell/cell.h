#pragma once

#include <algorithm>
#include <cmath>
#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>

#include "cell_genome.h"
#include "cell_settings.h"
#include "entities/body.h"
#include <array>
#include <cstdint>
#include <string.h>

#include "spring_links.h"

// Each organism consists of cells which work together via springs
// Each cell has their own radius and friction coefficient, as well as cosmetic factors such as color


// a faster implementation of the round function
inline float fast_round(float x)
{
	return x >= 0.0f ? floorf(x + 0.5f) : ceilf(x - 0.5f);
}

inline float fast_sqrt(float x)
{
	float y = x;

	// Initial approximation
	uint32_t i;
	std::memcpy(&i, &y, sizeof(float));

	i = (i + 0x3f800000) >> 1;

	std::memcpy(&y, &i, sizeof(float));

	// Newton refinement
	return 0.5f * (y + x / y);
}

inline float fast_sin(float x)
{
	constexpr float PI = 3.14159265358979323846f;
	constexpr float TWO_PI = 6.28318530717958647692f;

	// Wrap to [-PI, PI]
	x = x - TWO_PI * floorf((x + PI) / TWO_PI);

	// Bhaskara I approximation
	float y = (16.0f * x * (PI - fabsf(x))) /
		(5.0f * PI * PI - 4.0f * fabsf(x) * (PI - fabsf(x)));

	return y;
}


struct Cell : public CellGenome, CellSettings
{
private:
	bool reproduce_ = false; // signals to the protozoa manager that this cell needs an offspring index set
	bool dead_ = false;      // signals that the cell is in its decaying state

	float energy = initial_energy;
	float integrity = max_integrity;

public:
	uint32_t id_ = 0;        // The unique identifier for this cell
	uint32_t body_id_ = 0;   // Reference to the body 
	SpringLinks spring_links_; // The springs that connect this cell to other cells

	bool immortal_ = false;  // cell is unaffected by death

	// when a spring is created between this cell and other, the springs properties
	// will be average between the two cell's spring genomes
	SpringGenome spring_genome{};

	// Stomach and food
	uint16_t time_since_last_ate_ = 0;
	uint16_t repro_timer_ = 0;
	float nutrients_ = 0.f;
	uint8_t total_food_eaten_ = 0;

	float sinwave_current_friction_ = 0.f;

	// Statistics information
	uint16_t internal_clock_ = 0;
	uint8_t  offspring_count = 0;

	float cumulative_collision_damage_ = 0.f;
	float cumulative_spring_damage_ = 0.f;

	std::array<uint32_t, 8> nearby_food_ids_{};
	int nearby_food_ids_size_ = 0;

	// This keeps track of how many connections this cell has made as a newborn so we can limit it
	uint8_t new_connections_made = 0;


public:
	float delta_energy = 0.f;
	float delta_integrity = 0.f;

	// cell.h, public section
	std::array<uint32_t, max_cell_connections> connected_partner_ids_{};

	[[nodiscard]] bool already_connected_to(uint32_t other_id) const
	{
		for (uint8_t i = 0; i < new_connections_made; ++i)
			if (connected_partner_ids_[i] == other_id)
				return true;
		return false;
	}

	void register_connection(uint32_t other_id)
	{
		if (new_connections_made < max_cell_connections)
			connected_partner_ids_[new_connections_made++] = other_id;
	}

	Cell(const uint32_t body_id = 0)
	{
		body_id_ = body_id;
		spring_genome.randomize();
	}

	// ------------ Data accessors ------------
	[[nodiscard]] bool is_alive() const { return !dead_; }
	[[nodiscard]] bool can_die() const { return energy <= 0; }

	[[nodiscard]] bool should_reproduce() const { return reproduce_; }

	[[nodiscard]] sf::Color get_outer_color() const { return { outer_r, outer_g, outer_b, outer_transparency }; }
	[[nodiscard]] sf::Color get_inner_color() const { return { inner_r, inner_g, inner_b, inner_transparency }; }

	[[nodiscard]] sf::Vector2f get_pos_nearby(const Body* body, const float range) const;
	[[nodiscard]] float calculate_friction() const;

	[[nodiscard]] float get_integrity() const { return integrity; }
	[[nodiscard]] float get_energy() const { return energy; }

	// ------------ Data setters ------------
	void kill()
	{
		if (!immortal_)
			dead_ = true;
	}
	void force_reproduce() { reproduce_ = true; }
	void change_energy(const float amount) { delta_energy += amount; }
	void change_integrity(const float amount) { delta_integrity += amount; }
	void set_energy(const float amount) { energy = amount; }
	void set_integrity(const float amount) { integrity = amount; }

	// ------------ Cell functionality ------------
	void update_statistics();
	void update_organics(bool immune, bool friction_energy_loss, bool old_age_death);

	[[nodiscard]] bool eat(const float nutrients);
	void create_offspring(Body* this_body, Cell* child, Body* child_body, const bool mutate);
	static bool consume_food_check(const sf::Vector2f& cell_pos, const sf::Vector2f& food_pos, const float combined_rad);

	void recreate();
	void turn_off_reproduction();

	// reproductive
	bool check_sufficient_energy() const { return energy >= (birth_energy_thresh * max_energy); }
	bool check_sufficient_integrity() const { return integrity >= (birth_integrity_thresh * max_integrity); }
	bool check_sufficient_nutrients() const { return nutrients_ >= (birth_nutrients_thresh * max_nutrients); }
	bool check_repro_cooldown() const { return repro_timer_ >= repro_cooldown; }

	// ------------ Reproduction diagnostics (for debug UI) ------------
	// [0,1] progress toward each requirement; 1.0 == threshold met
	[[nodiscard]] float energy_progress()    const { return max_energy > 0.f ? std::clamp(energy / (birth_energy_thresh * max_energy), 0.f, 1.f) : 1.f; }
	[[nodiscard]] float integrity_progress() const { return max_integrity > 0.f ? std::clamp(integrity / (birth_integrity_thresh * max_integrity), 0.f, 1.f) : 1.f; }
	[[nodiscard]] float nutrients_progress() const { return max_nutrients > 0.f ? std::clamp(nutrients_ / (birth_nutrients_thresh * max_nutrients), 0.f, 1.f) : 1.f; }
	[[nodiscard]] float cooldown_progress()  const { return repro_cooldown > 0 ? std::clamp(static_cast<float>(repro_timer_) / static_cast<float>(repro_cooldown), 0.f, 1.f) : 1.f; }

	[[nodiscard]] float energy_threshold()    const { return birth_energy_thresh * max_energy; }
	[[nodiscard]] float integrity_threshold() const { return birth_integrity_thresh * max_integrity; }
	[[nodiscard]] float nutrients_threshold() const { return birth_nutrients_thresh * max_nutrients; }

private:
	// ------------ Organics sub-steps, run in this order by update_organics ------------
	void apply_immunity();
	void apply_passive_decay();
	void apply_old_age_decay();
	void flush_deltas();
	[[nodiscard]] bool check_death();       // returns true (and sets dead_) if energy or integrity hit 0
	void update_reproduction_flag();

	void process_nutrients();
	void repair_integrity();

	sf::Vector2f get_pos_nearby_min_max(const sf::Vector2f parent_pos, float min_radius, float max_radius);

};
