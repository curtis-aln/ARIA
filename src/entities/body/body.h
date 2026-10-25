#pragma once

#include <array>
#include <cstdint>
#include <SFML/System/Vector2.hpp>

/* Body holds the physical properties of a cell: position, velocity, mass and radius. */

using BodyId = uint32_t;

struct Body
{
	bool active = true;

	// This id references the body itself
	BodyId id_ = 0;

	// Keeping track of nearby ids to resolve collisions with
	std::array<BodyId, 8> nearby_ids_{};
	uint8_t nearby_ids_size_ = 0;

	sf::Vector2f position_{};
	sf::Vector2f velocity_{};
	float mass_ = 0.f;
	float radius_ = 0.f;

	float impulse_ = 0.f; // impulse applied to this body, used for collision damage

	// Radius <-> mass curve: radius = size_scale * mass^size_exponent.
	// Defined in body.cpp so tuning the anchors doesn't recompile the world.
	static const float size_exponent;
	static const float size_scale;

	explicit Body(BodyId id = 0) : id_(id) {}

	void set_radius(); // radius_ from mass_
	void set_mass();   // mass_ from radius_ (inverse of set_radius)

	static float mass_to_radius(const float mass);

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
		impulse_ = 0.f;          // a copy shouldn't inherit the parent's collision damage
		nearby_ids_size_ = 0;
	}
};