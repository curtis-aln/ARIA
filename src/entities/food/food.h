#pragma once

#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Color.hpp>
#include "entities/body.h"
#include "../../Utils/random.h"
#include "../../managers/food_manager/food_manager_settings.h"

/* FoodManager

+
*/

struct Food : FoodManagerSettings
{
	int id_ = 0; // unique food ID, relative to the food container
	int body_id_ = 0; // unique body ID, relative to the body container
	float age = 0;
	int time_since_last_reproduced = 0;

	float nutrients = initial_nutrients;

	sf::Color color{};

	bool active = true;

	float vibration_x = 0.f;
	float vibration_y = 0.f;

	void reset()
	{
		body_id_ = 0;
		age = 0;
		time_since_last_reproduced = 0;
		nutrients = initial_nutrients;
		color = {};
		active = true;
		vibration_x = 0.f;
		vibration_y = 0.f;
	}

	void update()
	{
		time_since_last_reproduced++;
		age += Random::rand_range(0.4f, 1.0f);

		update_food_nutrients();
		vibrate_food(vibration_strength);
	}

	bool is_food_dead() const
	{
		return nutrients < initial_nutrients && age > spawn_immunity;
	}

	float calculate_food_size()
	{
		// Radius is directly proportional to current nutrients
		if (nutrients == 0.f)
			return 0.f;

		return nutrients * nutrients_to_radius_scale;
	}

private:
	void update_food_nutrients()
	{
		// Nutrients develop from initial_nutrients toward final_nutrients over nutrient_development_time frames.
		// Once the target is reached, no further change is applied.
		// When the food is old enough to die, nutrients start dropping back down instead.

		const bool is_dying = age >= death_age;

		if (is_dying)
		{
			// Drain nutrients at the same rate they developed, until hitting initial_nutrients
			const float drain_rate = (final_nutrients - initial_nutrients)
				/ static_cast<float>(nutrient_development_time);

			nutrients -= drain_rate;
			return;
		}

		// Normal development toward final_nutrients
		const float diff = final_nutrients - initial_nutrients;
		const float increment = diff / static_cast<float>(nutrient_development_time);

		const bool increasing = diff > 0.f;
		const bool reached_target = increasing
			? nutrients >= final_nutrients
			: nutrients <= final_nutrients;

		if (reached_target)
			return;

		nutrients = std::clamp(
			nutrients + increment,
			std::min(initial_nutrients, final_nutrients),
			std::max(initial_nutrients, final_nutrients)
		);
	}

	

	void vibrate_food(float strength)
	{
		vibration_x = 0.f;
		vibration_y = 0.f;

		if (Random::rand01_float() < vibrate_freq)
			return;

		vibration_x = Random::rand11_float() * strength;
		vibration_y = Random::rand11_float() * strength;
	}
};