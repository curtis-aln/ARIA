#pragma once

/* Body class is a simple struct which holds the physical properties of a cell, such as position, velocity, and mass. */

struct Body
{
	bool active = true;

	// This id references the body itself
	uint32_t id_ = 0;

	inline static constexpr float density = 1.f; // the density of the cell, used to calculate radius from mass

	sf::Vector2f position_;
	sf::Vector2f velocity_;
	float mass_;
	float radius_;

	Body(uint32_t id = 0)
		: id_(id)
	{
		
	}

	void accelerate(const sf::Vector2f& acceleration)
	{
		velocity_ += acceleration;
	}

	void update_physics()
	{
		position_ += velocity_;
	}

	void reset()
	{
		velocity_ = { 0.f, 0.f };
	}
};