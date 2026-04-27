#pragma once
#include <SFML/System/Vector2.hpp>
#include <algorithm>

#include "../settings.h"
#include "cell.h"
#include "genetics/SpringGenome.h"

struct Spring : public SpringGenome
{
	// we store the id's of the cells here so whe we call update in the main class we know where to look for the cells, relative to the protozoa
	uint8_t cell_A_id{};
	uint8_t cell_B_id{};

	// unique spring ID, used for genome referencing, must not change during the spring's lifetime
	uint8_t id{};

	// for debugging
	sf::Vector2f direction_A_force{};
	sf::Vector2f direction_B_force{};
	float spring_length{};

	bool broken = false;

	Spring(const uint8_t _id, const uint8_t _cell_A_id, const uint8_t _cell_B_id)
		: cell_A_id(_cell_A_id), cell_B_id(_cell_B_id), id(_id), SpringGenome()
	{

	}

	float update(Cell& cell_a, Cell& cell_b, const int internal_clock)
	{
		const sf::Vector2f pos_a = cell_a.position_;
		const sf::Vector2f pos_b = cell_b.position_;
		const sf::Vector2f vel_a = cell_a.velocity_;
		const sf::Vector2f vel_b = cell_b.velocity_;

		const float dist = (pos_b - pos_a).length();
		spring_length = dist;

		if (dist < 1e-6f)
		{
			// i dont know why this happens so we will just kill the organism and let evolution fix the bug

			broken = true;
			return false;
		}


		// finding the rest length of the spring
		const float rest_length = calculate_rest_length(cell_a.radius, cell_b.radius, internal_clock);

		// Calculating the spring force: Fs = K * (|B - A| - L)
		const float spring_force = spring_const * (dist - rest_length);

		// Calculating the damping force
		const sf::Vector2f normalised_dir = ((pos_b - pos_a) / dist);
		const sf::Vector2f vel_difference = (vel_b - vel_a);
		const float damping_force = normalised_dir.dot(vel_difference) * damping;

		// Calculating total force (sum of the two forces)
		const float total_force = spring_force + damping_force;

		// moving each cell
		direction_A_force = total_force * ((pos_b - pos_a) / dist);
		cell_a.accelerate(direction_A_force);

		direction_B_force = total_force * ((pos_a - pos_b) / dist);
		cell_b.accelerate(direction_B_force);

		// finally we check if the spring should break (if its length surpasses breaking length) and return that information
		broken = dist > ProtozoaSettings::breaking_length;
		
		// we can calculate the amount of energy this contraction / extension took
		float work_done = std::abs(spring_force) * std::abs(dist - rest_length);
		work_done *= ProtozoaSettings::spring_work_const; 
		return work_done;
	}


private:
	float calculate_rest_length(const float rad_a, const float rad_b, const int internal_clock) const
	{
		float length_by_ratio = amplitude * sinf(frequency * internal_clock + offset) + vertical_shift;
		length_by_ratio = std::clamp(length_by_ratio, 0.f, 1.f);
		const float length_absolute_value = length_by_ratio * ProtozoaSettings::maximum_extension;
		return length_absolute_value;
	}
};
