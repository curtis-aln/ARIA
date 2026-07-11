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

	void update_physics(Body& body_a, Body& body_b)
	{
		clock_++;

		const sf::Vector2f& pos_a = body_a.position_;
		const sf::Vector2f& pos_b = body_b.position_;
		const sf::Vector2f& vel_a = body_a.velocity_;
		const sf::Vector2f& vel_b = body_b.velocity_;

		current_length = (pos_b - pos_a).length();

		if (current_length > SPRING_BREAK_LENGTH)
		{
			break_spring();
		}

		// finding the rest length of the spring
		rest_length = calculate_rest_length(clock_);

		// Calculating the spring force: Fs = K * (|B - A| - L)
		spring_force = spring_const * (current_length - rest_length);

		// Calculating the damping force
		const sf::Vector2f normalised_dir = ((pos_b - pos_a) / current_length);
		const sf::Vector2f vel_difference = (vel_b - vel_a);
		damping_force = normalised_dir.dot(vel_difference) * damping;

		// Calculating total force (sum of the two forces)
		const float total_force = spring_force + damping_force;

		// moving each cell
		body_a.accelerate(total_force * ((pos_b - pos_a) / current_length));
		body_b.accelerate(total_force * ((pos_a - pos_b) / current_length));

		// we can calculate the amount of energy this contraction / extension took
		work_done = std::abs(spring_force) * std::abs(current_length - rest_length);
		work_done *= SPRING_WORK_CONST;

		const float force_magnitude = std::abs(total_force);


		// Stress: 0 = relaxed, 1 = at breaking point
		stress = force_magnitude / SPRING_BREAK_FORCE;

		// Force-based break (complements your existing length-based break)
		if (force_magnitude > SPRING_BREAK_FORCE)
		{
			break_spring();
		}

	}

	void update_organics(Cell& cell_a, Cell& cell_b)
	{
		if (stress > SPRING_DAMAGE_THRESH)
		{
			float excess = stress - SPRING_DAMAGE_THRESH;
			cell_a.integrity -= excess;
			cell_b.integrity -= excess;
		}

		handle_reproduction(cell_a, cell_b);

		transfer_nutrients(cell_a, cell_b);

		cell_a.energy -= work_done / 2.f; // Eventually springs will be their own organic systems with energy
		cell_b.energy -= work_done / 2.f;
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

	void transfer_nutrients(Cell& cell_a, Cell& cell_b)
	{
		// you cant transfer nutrients to a cell which is decaying
		if (!cell_a.is_alive() || !cell_b.is_alive())
			return;

		const float transfer = std::copysign(nutrient_transfer_rate, cell_b.nutrients_ - cell_a.nutrients_);
		cell_a.nutrients_ += transfer;
		cell_b.nutrients_ -= transfer;
	}

	float calculate_rest_length(const int internal_clock)
	{
		// sin oscillates around vertical_shift with ±amplitude swing
		const float sin_value = sinf(frequency * internal_clock + offset); // [-1, 1]
		ratio = vertical_shift + amplitude * sin_value;     // [vs-a, vs+a]
		const float clamped = std::clamp(ratio, 0.f, 1.f);
		return clamped * maximum_extension;
	}
};
