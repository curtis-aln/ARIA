#pragma once

#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Color.hpp>
#include <cmath>
#include <algorithm>

#include "settings.h"
#include "genetics/CellGenome.h"
#include "../Utils/Graphics/Circle.h"

// Each organism consists of cells which work together via springs
// Each cell has their own radius and friction coefficient, as well as cosmetic factors such as color


struct Cell : public CellGenome
{
private:
	uint16_t clock_ = 0;

protected:
	sf::Vector2f position_{};
	sf::Vector2f velocity_{};

public:
	// The Cell ID is used when referencing the cell inside the protozoa, and identifying its genome
	uint8_t id{}; 
	float energy = ProtozoaSettings::initial_energy;

	// Stomach and food
	uint16_t time_since_last_ate_ = 0;
	uint16_t time_since_last_reproduced_ = 0;
	float nutrients_ = 0.f;
	uint8_t total_food_eaten_ = 0;
	uint8_t stomach_ = 0;
	float integrity = ProtozoaSettings::integrity;

	// Statistics information
	uint16_t frames_alive_ = 0;


	Cell(const uint32_t _id = 0, const sf::Vector2f position = {0, 0})
		: position_(position), id(_id)
	{
		
	}

	[[nodiscard]] const sf::Vector2f& get_pos() const { return position_; }
	[[nodiscard]] const sf::Vector2f& get_vel() const { return velocity_; }

	void set_pos(const sf::Vector2f& position) { position_ = position;}

	[[nodiscard]] sf::Color get_outer_color() const { return { outer_r, outer_g, outer_b, transparency }; }
	[[nodiscard]] sf::Color get_inner_color() const { return { inner_r, inner_g, inner_b, transparency }; }

	void reset()
	{
		energy = ProtozoaSettings::initial_energy;
		clock_ = 0;
		generation = 0;
		time_since_last_ate_ = 0;
		nutrients_ = 0.f;
		total_food_eaten_ = 0;
		stomach_ = 0;
		frames_alive_ = 0;
		integrity = ProtozoaSettings::integrity;

		velocity_ = { 0, 0 };
	}

	void eat(const float nutrients)
	{
		nutrients_ += nutrients;
		//stomach_++; ToDO implement stomach

		time_since_last_ate_ = 0;
		++total_food_eaten_;
	}

	static bool consume_food_check(const Cell& cell, const sf::Vector2f& food_pos)
	{
		const float distance_sq = (food_pos - cell.position_).lengthSquared();
		const float rad = cell.radius + FoodSettings::food_radius;

		if (distance_sq > rad * rad)
			return false;
		return true;
	}


	void update()
	{
		update_statistics();
		update_organics();
		update_physics();
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
			const float k = WorldSettings::border_repulsion_magnitude;
			const float diff = dist - bounds_radius;
			accelerate(-normal * k * diff);
			
		}
	}

	void accelerate(const sf::Vector2f acceleration)
	{
		velocity_ += acceleration;
	}


	void create_offspring(Cell* child, const bool dormant = false, const bool mutate=true)
	{
		// dormant means the child cell will not tug or pull on the protozoa
		// if dormant is false the child will take on the mutations of the parent
		// if mutate is true then the child will go through mutation 

		// When creating an offspring, this is ran for every cell in the protozoa
		child->position_ = get_pos_nearby(2.f);

		if (dormant)
			create_dormant_cell(child);
		else
		{
			
		}
	}

	[[nodiscard]] bool can_die() const
	{
		if (energy <= 0 || integrity <= 0)
		{
			return true;
		}
		return false;
	}

	[[nodiscard]] bool can_reproduce() const
	{
		if (energy < ProtozoaSettings::reproduce_energy_thresh)
			return false;

		if (time_since_last_reproduced_ < ProtozoaSettings::reproductive_cooldown)
			return false;

		return true;
	}


	// When a child cell is created, it should be spawned somewhere near the parent cell.
	[[nodiscard]] sf::Vector2f get_pos_nearby(const float range) const
	{
		// Range in terms of radii
		const sf::FloatRect spawn_area = {
		{position_.x - radius * range, position_.y - radius * range},
		{radius * range *2 , radius * range * 2}
		};
		return Random::rand_pos_in_rect(spawn_area);
	}

	static void create_dormant_cell(Cell* child)
	{
		// Dormant cells are cells which do not tug or pull on the protozoa organism as they have little to no friction
		// This is so that when protozoa's decide to add cells to their body their movement isn't drastically affected

		child->amplitude = 0.01f;
		child->frequency = 1.f;
		child->vertical_shift = CellGeneticConstraints::vertical_shift.max;
		child->offset = 0.f;
	}

	[[nodiscard]] float calculate_friction() const
	{
		const float friction = amplitude * sinf(frequency * clock_ + offset) + vertical_shift;

		// clamping friction to [0, 1]
		return std::clamp(friction, 0.f, 1.f);
	}

private:
	void update_statistics()
	{
		clock_++;
		time_since_last_ate_++;
		frames_alive_++;
		time_since_last_reproduced_++;
	}


	void update_organics()
	{
		convert_nutrients_to_energy();
		convert_nutrients_to_integrity();
		energy -= ProtozoaSettings::energy_decay_rate;
	}

	void update_physics()
	{
		const float friction = calculate_friction();

		// updating the velocity with the new friction
		velocity_ *= friction;

		position_ += velocity_;
	}

	void convert_nutrients_to_integrity()
	{
		if (energy < ProtozoaSettings::integrity_conversion_rate)
			return;

		integrity += ProtozoaSettings::integrity_conversion_rate;
		energy -= ProtozoaSettings::integrity_conversion_rate;
	}


	void convert_nutrients_to_energy()
	{
		if (nutrients_ < ProtozoaSettings::conversion_rate)
			return;

		energy += ProtozoaSettings::conversion_rate;
		nutrients_ -= ProtozoaSettings::conversion_rate;
	}

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