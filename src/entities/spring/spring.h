#pragma once
#include <algorithm>
#include <SFML/System/Vector2.hpp>

#include "../cell/cell.h"
#include "spring_settings.h"
#include <cstdint>
#include <cstdint>
#include <cstdlib>
#include <cstdlib>
#include <entities/cell/cell_genome.h>
#include <entities/cell/cell_genome.h>
#include <entities/cell/cell_settings.h>
#include <entities/cell/cell_settings.h>
#include <Utils/random.h>
#include <Utils/random.h>

struct SpringResult { float work_done; float force_magnitude; bool broken; };

struct Spring : SpringSettings
{
private:
	bool broken = false;

public:
	// unique spring ID, used for genome referencing, must not change during the spring's lifetime
	uint32_t id_{};

	// The springs genetic information, determined by its two connecting cells
	SpringGenome genome{};

	// we store the id's of the cells here so whe we call update in the main class we know where to look for the cells, relative to the protozoa
	uint32_t cell_A_id{};
	uint32_t cell_B_id{};

	uint16_t internal_clock_{};
	float death_chance_ = 0.f; // zero means no chance of breaking, 1 means guaranteed breakage on the first frame

	float work_done = 0.f;
	float rest_length = 0.f;
	float current_length = 0.f;
	float ratio = 0.f;

	float spring_force = {};
	float damping_force = {};

	float stress = 0.f; // 0..1, normalised force relative to break threshold

	sf::Vector2f movement_vector{ 0 ,0 };



	Spring(const uint8_t _id = 0, const uint8_t _cell_A_id = 0, const uint8_t _cell_B_id = 0)
		: cell_A_id(_cell_A_id), cell_B_id(_cell_B_id), id_(_id)
	{

	}

	void reset()
	{
		cell_A_id = 0;
		cell_B_id = 0;

		internal_clock_ = 0;
		work_done = 0.f;
		current_length = 0.f;
		rest_length = 0.f;
		stress = 0.f;
		broken = false;
		spring_force = 0.f;
		damping_force = 0.f;
		ratio = 0.f;

		genome.frequency = 0.f;
		genome.offset = 0.f;
		genome.vertical_shift = 1.f;
		genome.amplitude = 0.f;
		genome.spring_const = 0.f;
		genome.damping = 0.f;
		genome.nutrient_transfer_rate = 0.f;

		movement_vector = { 0, 0 };

		death_chance_ = 0.f;
	}

	void break_spring()
	{
		broken = true;
	}

	bool is_spring_broken()
	{
		return broken;
	}

	void create_offspring(Spring& offspring)
	{
		offspring.genome.copy_genetics(genome);
	}

	// returns a movement vector
	void update_physics(
		const sf::Vector2f& pos_a, const sf::Vector2f& vel_a,
		const sf::Vector2f& pos_b, const sf::Vector2f& vel_b,
		bool disable_length_breakage, bool disable_force_breakage)
	{
		internal_clock_++;

		const sf::Vector2f dir = pos_b - pos_a;
		const float length_squared = dir.x * dir.x + dir.y * dir.y;

		float break_len = spring_break_length_factor * maximum_extension;
		if (length_squared > break_len * break_len && !disable_length_breakage)
		{
			break_spring();
			movement_vector = { 0, 0 };
			return;
		}

		// finding the rest length of the spring
		rest_length = calculate_rest_length(internal_clock_);

		current_length = fast_sqrt(length_squared);
		const float length_diff = current_length - rest_length;
		const float inv_length = 1.0f / current_length;


		// Calculating the spring force: Fs = K * (|B - A| - L)
		spring_force = get_spring_constant() * length_diff;

		// Calculating the damping force
		const sf::Vector2f normalised_dir{ dir.x * inv_length, dir.y * inv_length };
		const sf::Vector2f vel_difference = (vel_b - vel_a);
		damping_force = normalised_dir.dot(vel_difference) * genome.damping;

		// Calculating total force (sum of the two forces)
		const float total_force = spring_force + damping_force;
		movement_vector = normalised_dir * total_force;

		// we can calculate the amount of energy this contraction / extension took
		work_done = std::abs(spring_force * length_diff);
		work_done *= spring_work_const;

		const float force_magnitude = std::abs(total_force);

		// Stress: 0 = relaxed, 1 = at breaking point
		stress = force_magnitude / spring_break_force;

		// Force-based break (complements your existing length-based break)
		if (force_magnitude > spring_break_force && !disable_force_breakage)
			break_spring();
	}

	void death_update(Cell& cell_a, Cell& cell_b)
	{
		if (Random::rand01_float() < death_chance_)
		{
			broken = true;
			return;
		}

		if (!cell_a.is_alive() || !cell_b.is_alive())
		{
			broken = true;
			return;
		}
	}

	void update_organics(Cell& cell_a, Cell& cell_b, bool stress_damage, bool work_done_energy)
	{
		death_update(cell_a, cell_b);

		if (stress > spring_damage_threshold && stress_damage)
		{
			update_integrity(cell_a, cell_b);
		}

		transfer_nutrients(cell_a, cell_b);

		if (work_done_energy)
		{
			float energy_cost = -work_done / 2.f;
			cell_a.change_energy(energy_cost);
			cell_b.change_energy(energy_cost);
		}
	}

	void update_integrity(Cell& cell_a, Cell& cell_b)
	{
		float excess = stress - spring_damage_threshold;
		float damage = -excess / 2.f * stress_damage_const;

		cell_a.change_integrity(damage);
		cell_b.change_integrity(damage);
		cell_a.cumulative_spring_damage_ += abs(damage);
		cell_b.cumulative_spring_damage_ += abs(damage);
	}

private:
	[[nodiscard]] float get_spring_constant()
	{
		const float growth = std::clamp(static_cast<float>(internal_clock_) / fully_developed_age, 0.f, 1.f);
		return genome.spring_const * growth;
	}


	// takes in the nutrients of cell a and cell b and transfers between them,
// respecting max_nutrients caps and applying a fixed % loss per transfer
	// transfer_nutrients() — full new body
	void transfer_nutrients(Cell& cell_a, Cell& cell_b)
	{
		const float rate = std::max(genome.nutrient_transfer_rate, 0.0f);

		if (rate <= 0.0f)
			return;

		float& nutrients_a = cell_a.nutrients_;
		float& nutrients_b = cell_b.nutrients_;

		// "recently reproduced" == still inside its post-reproduction cooldown window
		const bool a_reproduced = !cell_a.check_repro_cooldown();
		const bool b_reproduced = !cell_b.check_repro_cooldown();

		float* sender;
		float* receiver;
		bool priority_transfer = false;

		if (a_reproduced != b_reproduced)
		{
			// exactly one cell has recently reproduced -> prioritise nutrients toward the one that hasn't
			float* desired_sender = a_reproduced ? &nutrients_a : &nutrients_b;
			float* desired_receiver = a_reproduced ? &nutrients_b : &nutrients_a;

			// what would the plain nutrient-gradient logic pick?
			float* gradient_sender = (nutrients_b > nutrients_a) ? &nutrients_b : &nutrients_a;

			sender = desired_sender;
			receiver = desired_receiver;

			// if gradient logic isn't already sending this way, we're overriding it -
			// skip the diff>0 gradient check below, since the reproduced cell may well
			// have fewer nutrients than the cell we're prioritising
			priority_transfer = (gradient_sender != desired_sender);
		}
		else
		{
			sender = (nutrients_b > nutrients_a) ? &nutrients_b : &nutrients_a;
			receiver = (nutrients_b > nutrients_a) ? &nutrients_a : &nutrients_b;
		}

		const float diff = *sender - *receiver;
		if (diff <= 0.0f && !priority_transfer)
			return;

		const float space = CellSettings::max_nutrients - *receiver;
		if (space <= 0.0f)
			return;

		// never drain the sender below the protected floor - this is what lets the priority
		// transfer above run without starving whichever cell just reproduced
		const float sendable = *sender;

		const float send_amount = priority_transfer
			? std::min({ rate, sendable, space / (1.0f - nutrients_transfer_loss) })
			: std::min({ rate, sendable, diff, space / (1.0f - nutrients_transfer_loss) });

		const float receive_amount = send_amount * (1.0f - nutrients_transfer_loss);

		*sender -= send_amount;
		*receiver += receive_amount;
	}

	float calculate_rest_length(const int internal_clock)
	{
		// sin oscillates around vertical_shift with ±amplitude swing
		const float sin_value = fast_sin(genome.frequency * internal_clock + genome.offset); // [-1, 1]
		ratio = genome.vertical_shift + genome.amplitude * sin_value;     // [vs-a, vs+a]
		const float clamped = std::clamp(ratio, 0.f, 1.f);
		return clamped * maximum_extension;
	}
};
