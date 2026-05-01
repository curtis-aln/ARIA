#pragma once
#include <SFML/System/Vector2.hpp>
#include <algorithm>

#include "../settings.h"
#include "cell.h"
#include "genetics/SpringGenome.h"

struct SpringResult { float work_done; bool broken; };

struct Spring : SpringGenome
{
	// we store the id's of the cells here so whe we call update in the main class we know where to look for the cells, relative to the protozoa
	uint8_t cell_A_id{};
	uint8_t cell_B_id{};

	// unique spring ID, used for genome referencing, must not change during the spring's lifetime
	uint8_t id{};

	uint16_t clock_{};

	float work_done = 0.f;
	float rest_length = 0.f;
	float current_length = 0.f;

	float spring_force = {};
	float damping_force = {};

	Spring(const uint8_t _id, const uint8_t _cell_A_id, const uint8_t _cell_B_id)
		: cell_A_id(_cell_A_id), cell_B_id(_cell_B_id), id(_id), SpringGenome()
	{

	}


	void reset()
	{
		clock_ = 0;
	}


	SpringResult update(Cell& cell_a, Cell& cell_b)
	{
		clock_++; 

		const sf::Vector2f& pos_a = cell_a.get_pos();
		const sf::Vector2f& pos_b = cell_b.get_pos();
		const sf::Vector2f& vel_a = cell_a.get_vel();
		const sf::Vector2f& vel_b = cell_b.get_vel();

		current_length = (pos_b - pos_a).length();

		if (current_length < 1e-6f || current_length > ProtozoaSettings::breaking_length)
		{
			return {.work_done = 0.f, .broken = true };
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
		cell_a.accelerate(total_force * ((pos_b - pos_a) / current_length));
		cell_b.accelerate(total_force * ((pos_a - pos_b) / current_length));

		// finally we check if the spring should break (if its length surpasses breaking length) and return that information
		bool broken = current_length > ProtozoaSettings::breaking_length;
		
		// we can calculate the amount of energy this contraction / extension took
		work_done = std::abs(spring_force) * std::abs(current_length - rest_length);
		work_done *= ProtozoaSettings::spring_work_const; 

		return { work_done, broken };
	}

	void mutate(float mut_rate = 0.f, float mut_range = 0.f)
	{
		auto chance = [](float rate) { return Random::rand01_float() < rate; };
		auto rand_sym = [](float range) { return Random::rand_range(-range, range); };

		// if the defualt mr is zero we set it t othe cells mr
		mut_rate = (mut_rate == 0.f) ? mutation_rate : mut_rate;
		mut_range = (mut_range == 0.f) ? mutation_range : mut_range;

		amplitude += chance(mut_rate) ? rand_sym(mut_range) : 0.f;
		frequency += chance(mut_rate) ? rand_sym(mut_range) : 0.f;
		offset += chance(mut_rate) ? rand_sym(mut_range) : 0.f;
		vertical_shift += chance(mut_rate) ? rand_sym(mut_range) : 0.f;

		spring_const += chance(mut_rate) ? rand_sym(mut_range) : 0.f;
		damping += chance(mut_rate) ? rand_sym(mut_range) : 0.f;

		mutation_range += chance(mutation_rate) ? rand_sym(mutation_rate_range) : 0.f;
		mutation_rate += chance(mutation_rate) ? rand_sym(mutation_rate_range) : 0.f;

		// handle clamping of the params
		spring_const = std::clamp(spring_const, 0.f, max_spring_const);
		damping = std::clamp(damping, 0.f, max_damping);
		amplitude = std::clamp(amplitude, 0.f, max_amplitude);
		frequency = std::clamp(frequency, -max_frequency, max_frequency);
		offset = std::clamp(offset, -max_offset, max_offset);
		vertical_shift = std::clamp(vertical_shift, -max_vertical_shift, max_vertical_shift);
	}


private:
	float calculate_rest_length(const int internal_clock) const
	{
		float length_by_ratio = amplitude * sinf(frequency * internal_clock + offset) + vertical_shift;
		length_by_ratio = std::clamp(length_by_ratio, 0.f, 1.f);
		const float length_absolute_value = length_by_ratio * ProtozoaSettings::maximum_extension;
		return length_absolute_value;
	}
};
