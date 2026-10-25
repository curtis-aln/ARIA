#pragma once

/* Body class is a simple struct which holds the physical properties of a cell, such as position, velocity, and mass. */

using BodyId = uint32_t;

static inline const float size_exponent = std::log(135.f / 55.f) / std::log(150.f / 50.f); // ≈ 0.817
static inline const float size_scale = 55.f / std::pow(50.f, size_exponent);

struct Body
{
	bool active = true;

	// This id references the body itself
	BodyId id_ = 0;

	// Keeping track of nearby id's to resolve collisions with
	std::array<BodyId, 8> nearby_ids_;
	uint8_t nearby_ids_size_ = 0;

	inline static constexpr float density = 0.5f; // the density of the cell, used to calculate radius from mass

	sf::Vector2f position_;
	sf::Vector2f velocity_;
	float mass_;
	float radius_;

	float impulse_ = 0.f; // the impulse applied to this body, used to calculate the force applied to it

	Body(BodyId id = 0)
		: id_(id)
	{
		
	}

	void set_radius()
	{
		radius_ = size_scale * std::pow(mass_, size_exponent);
	}

	void set_mass()   // inverse, keep the two in sync
	{
		mass_ = std::pow(radius_ / size_scale, 1.f / size_exponent);
	}

	void accelerate(const sf::Vector2f& acceleration)
	{
		velocity_ += acceleration;
	}

	void update_physics()
	{
		position_ += velocity_;
	}

	void reset_cell_manager()
	{
		velocity_ = { 0.f, 0.f };
		nearby_ids_size_ = 0;
		impulse_ = 0.f;
	}

	void copy(const Body* other)
	{
		position_ = other->position_;
		velocity_ = other->velocity_;
		mass_ = other->mass_;
		radius_ = other->radius_;
		nearby_ids_size_ = 0;
	}

};