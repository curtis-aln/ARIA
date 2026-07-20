#pragma once
#include <SFML/System/Vector2.hpp>
#include <algorithm>

#include "../cell/cell.h"
#include "spring_genome.h"
#include "spring_settings.h"

struct SpringResult { float work_done; float force_magnitude; bool broken; };

struct Spring : SpringGenome, SpringSettings
{
	// These paramaters determine the organics of the spring
	inline static float SPRING_BREAK_FORCE = 0.f;
	inline static float SPRING_BREAK_LENGTH = 0.f;
	inline static float SPRING_DAMAGE_THRESH = 0.f;
	inline static float SPRING_WORK_CONST = 0.f;

private:
	bool broken = false;

public:
	// unique spring ID, used for genome referencing, must not change during the spring's lifetime
	uint32_t id_{};

	// we store the id's of the cells here so whe we call update in the main class we know where to look for the cells, relative to the protozoa
	uint32_t cell_A_id{};
	uint32_t cell_B_id{};

	uint16_t clock_{};

	float work_done = 0.f;
	float rest_length = 0.f;
	float current_length = 0.f;
	float ratio = 0.f;

	float spring_force = {};
	float damping_force = {};

	float stress = 0.f; // 0..1, normalised force relative to break threshold

	sf::Vector2f movement_vector{ 0 ,0 };

	

	Spring(const uint8_t _id=0, const uint8_t _cell_A_id=0, const uint8_t _cell_B_id=0)
		: cell_A_id(_cell_A_id), cell_B_id(_cell_B_id), id_(_id), SpringGenome()
	{

	}

	void reset()
	{
		clock_ = 0;
		work_done = 0.f;
		current_length = 0.f;
		broken = false;
		spring_force = 0.f;
		damping_force = 0.f;
		ratio = 0.f;

		frequency = 0.f;
		offset = 0.f;
		vertical_shift = 1.f;
		amplitude = 0.f;
		movement_vector = { 0, 0 };
	}

	void break_spring()
	{
		// springs have a spawn immunity
		if (clock_ > 30)
			broken = true;
		else
			broken = false;
	}

	bool is_spring_broken()
	{
		return broken;
	}

	void create_offspring(Spring& offspring)
	{
		offspring.copy_genetics(*this);
		offspring.generation++;
	}

	// returns a movement vector
	void update_physics(const sf::Vector2f& pos_a, const sf::Vector2f& vel_a, const sf::Vector2f& pos_b, const sf::Vector2f& vel_b)
	{
		clock_++;

		const sf::Vector2f dir = pos_b - pos_a;
		const float length_squared = dir.x * dir.x + dir.y * dir.y;

		if (length_squared > SPRING_BREAK_LENGTH * SPRING_BREAK_LENGTH)
		{
			break_spring();
		}

		current_length = fast_sqrt(length_squared);
		const float length_diff = current_length - rest_length;
		const float inv_length = 1.0f / current_length;

		// finding the rest length of the spring
		rest_length = calculate_rest_length(clock_);

		// Calculating the spring force: Fs = K * (|B - A| - L)
		spring_force = spring_const * (length_diff);

		// Calculating the damping force
		const sf::Vector2f normalised_dir{ dir.x * inv_length, dir.y * inv_length};
		const sf::Vector2f vel_difference = (vel_b - vel_a);
		damping_force = normalised_dir.dot(vel_difference) * damping;

		// Calculating total force (sum of the two forces)
		const float total_force = spring_force + damping_force;
		movement_vector = normalised_dir * total_force;

		// we can calculate the amount of energy this contraction / extension took
		work_done = std::abs(spring_force * length_diff);
		work_done *= SPRING_WORK_CONST;

		const float force_magnitude = std::abs(total_force);

		// Stress: 0 = relaxed, 1 = at breaking point
		stress = force_magnitude / SPRING_BREAK_FORCE;

		// Force-based break (complements your existing length-based break)
		if (force_magnitude > SPRING_BREAK_FORCE)
			break_spring();
	}

	void update_organics(Cell& cell_a, Cell& cell_b)
	{
		if (!cell_a.is_alive() || !cell_b.is_alive())
			broken = true;

		if (stress > SPRING_DAMAGE_THRESH)
		{
			update_integrity(cell_a.integrity, cell_b.integrity);
		}

		handle_reproduction(cell_a, cell_b);

		float transfer = transfer_nutrients(cell_a.nutrients_, cell_b.nutrients_);

		cell_a.nutrients_ += transfer;
		cell_b.nutrients_ -= transfer;

		cell_a.energy -= work_done / 2.f; // Eventually springs will be their own organic systems with energy
		cell_b.energy -= work_done / 2.f;
	}

	void update_integrity(float& cell_a_integrity, float& cell_b_integrity)
	{
		float excess = stress - SPRING_DAMAGE_THRESH;
		cell_a_integrity -= excess;
		cell_b_integrity -= excess;
	}

private:
	void handle_reproduction(Cell& cell_a, Cell& cell_b)
	{
		if (cell_a.offspring_index >= 0 && cell_b.offspring_index >= 0)
		{
			cell_a.connection_index = cell_b.offspring_index;
			cell_a.spring_to_copy_index = id_;
		}
	}

	// takes in the nutrients of cell a and cell b and reutrns the transfer amount
	float transfer_nutrients(float nutrients_a, float nutrients_b)
	{
		return std::copysign(nutrient_transfer_rate, nutrients_b - nutrients_a);
	}

	float calculate_rest_length(const int internal_clock)
	{
		// sin oscillates around vertical_shift with ±amplitude swing
		const float sin_value = fast_sin(frequency * internal_clock + offset); // [-1, 1]
		ratio = vertical_shift + amplitude * sin_value;     // [vs-a, vs+a]
		const float clamped = std::clamp(ratio, 0.f, 1.f);
		return clamped * maximum_extension;
	}
};
