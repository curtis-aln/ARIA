#pragma once
#include <algorithm>
#include <iostream>

#include "SFML/System/Vector2.hpp"
#include "Utils/random.h"

/*
The World border is quite important in this simulation as it determines how sparse the world matter is
this class has the ability to change size at a fixed velocity, and repel protozoa from the border
*/

struct WorldBorderSettings
{
	inline static float border_repulsion_magnitude; // how strong it is repelled from the border

	inline static float minimum_border_size = 500.f;
	inline static float maximum_border_size = 100'000.f;
};

struct WorldBorder : WorldBorderSettings
{
	sf::Vector2f center_{};
	float border_velocity = 0.f; // how fast the border changes size
	float bounds_radius;

public:
	WorldBorder(sf::Vector2f center = { 0, 0 }, float radius = 0.f)
		: center_(center), bounds_radius(radius)
	{
		
	}

	void update(const float dt)
	{
		bounds_radius += border_velocity * dt;
		bounds_radius = std::max(bounds_radius, minimum_border_size);
		bounds_radius = std::min(bounds_radius, maximum_border_size);
	}


	[[nodiscard]] bool contains(const sf::Vector2f& point) const
	{
		// if the distance from the center squared is greater than the radius squared it is out of bounds
		const sf::Vector2f delta = point - center_;
		const float dist_sq = delta.x * delta.x + delta.y * delta.y;
		const float rad_sq = bounds_radius * bounds_radius;

		return dist_sq < rad_sq;
	}

	[[nodiscard]] const sf::Vector2f& rand_pos() const
	{
		const sf::Rect<float> bounds = { {center_.x - bounds_radius, center_.y - bounds_radius}, {bounds_radius * 2, bounds_radius * 2} };
		for (int i = 0; i < 20; ++i)
		{
			if (const sf::Vector2f pos = Random::rand_pos_in_rect(bounds); contains(pos))
			{
				return pos;
			}
		}

		std::cout << "ERROR Circle.h rand_pos_in_circle() failed\n";
		return { 0, 0 };
	}
};