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
		const float transfer = std::copysign(ProtozoaSettings::energy_share_rate, cell_b.energy - cell_a.energy);
		cell_a.energy += transfer;
		cell_b.energy -= transfer;

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

		// we can calculate the amount of energy this contraction / extension took
		work_done = std::abs(spring_force) * std::abs(current_length - rest_length);
		work_done *= ProtozoaSettings::spring_work_const; 

		return { work_done, false };
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
