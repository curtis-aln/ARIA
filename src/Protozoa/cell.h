#pragma once

#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Color.hpp>
#include <cmath>
#include <algorithm>

#include "genetics/CellGenome.h"
#include "../Utils/Graphics/Circle.h"
#include "../Food/food_manager.h"

// Each organism consists of cells which work together via springs
// Each cell has their own radius and friction coefficient, as well as cosmetic factors such as color


struct Cell : public CellGenome
{
	// The Cell ID is used when referencing the cell inside the protozoa, and identifying its genome
	uint8_t id{}; 

	float sinwave_current_friction_ = 0.f;
	uint16_t time_since_last_ate = 0;

	float nutrients_eaten = 0.f; // The amount of nutrients this cell in particular has eaten
	uint8_t food_eaten = 0;

	sf::Vector2f position_{};
	sf::Vector2f velocity_{};

	Cell(const uint32_t _id, const sf::Vector2f position)
		: id(_id), position_(position), CellGenome()
	{
		
	}

	sf::Color get_outer_color() const { return { outer_r, outer_g, outer_b, transparency }; }
	sf::Color get_inner_color() const { return { inner_r, inner_g, inner_b, transparency }; }

	void reset()
	{
		generation = 0;
		sinwave_current_friction_ = 0.f;
		time_since_last_ate = 0;
		nutrients_eaten = 0.f;
		food_eaten = 0;
	}

	void eat(const float nutrients)
	{
		nutrients_eaten += nutrients;
		time_since_last_ate = 0;
		++food_eaten;
	}

	static bool consume_food_check(const Cell& cell, const Food* food)
	{
		sf::Vector2f food_pos = food->position;
		const float distance_sq = (food_pos - cell.position_).lengthSquared();
		const float rad = cell.radius + FoodSettings::food_radius;

		if (distance_sq > rad * rad)
			return false;
		return true;
	}


	void update(const int internal_clock)
	{
		time_since_last_ate++;
		// updating velocity and position vectors

		sinwave_current_friction_ = amplitude * sinf(frequency * internal_clock + offset) + vertical_shift;

		// clamping friction to [0, 1]
		sinwave_current_friction_ = std::clamp(sinwave_current_friction_, 0.f, 1.f);
		
		// updating the velocity with the new friction
		velocity_ *= sinwave_current_friction_;

		position_ += velocity_;
	}



	void bound(const Circle& bounds)
	{

		const sf::Vector2f direction = position_ - bounds.center;
		const float dist_sq = (position_ - bounds.center).lengthSquared();
		const float bounds_radius = bounds.radius - radius - radius * 0.1f; // Ensure the entire circle stays inside
		const float bounds_radius_sq = bounds_radius * bounds_radius;

		if (dist_sq > bounds_radius_sq) // Outside the boundary
		{
			const float dist = std::sqrt(dist_sq);
			sf::Vector2f normal = direction / dist; // Normalize direction

			// Move the circle back inside
			position_ = bounds.center + normal * bounds_radius;

			// Apply velocity adjustment to prevent escaping
			velocity_ -= normal * (WorldSettings::border_repulsion_magnitude * (dist - bounds_radius));
		}
	}

	void accelerate(const sf::Vector2f acceleration)
	{
		velocity_ += acceleration;
	}


private:
	static void clamp_velocity(sf::Vector2f& velocity)
	{
		const float speed_squared = velocity.x * velocity.x + velocity.y * velocity.y;

		if (speed_squared > WorldSettings::max_speed * WorldSettings::max_speed)
		{
			const float speed = sqrt(speed_squared);
			velocity *= WorldSettings::max_speed / speed;
		}
	}
};