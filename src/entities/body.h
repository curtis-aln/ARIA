#pragma once

/* Body class is a simple struct which holds the physical properties of a cell, such as position, velocity, and mass. */

struct Body
{
	bool active = true;

	uint32_t id_ = 0;

	inline static constexpr float density = 1.f; // the density of the cell, used to calculate radius from mass

	sf::Vector2f position_;
	sf::Vector2f velocity_;
	float mass_;
	float radius_;

	Body()
	{
		
	}
};