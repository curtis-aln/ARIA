#pragma once

#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Color.hpp>
#include "entities/body.h"
#include "../../Utils/random.h"
#include "../../managers/food_manager/food_manager_settings.h"

/* FoodManager

Food:
Reproduction:
When a food is born it can only reproduce when
1. its age surpasses the reproductive_threshold
2. it hasnt reproduced in a specified amount of frames (reproductive_cooldown)
3. The world food limit hasnt already been reached
- The chance of a food spawning is proportional to how much food there is in the world,
the more food there is the less chance there is for a new one to spawn.
- Every food item gets an opportunity to reproduce
- when a new food is made, it is spawned near its parent

Death:
- Food has a chance to die every frame after it reaches a certain age (death_age)
- it can also die by beng eaten by a protozoa

Nutrition:
- Every food starts with a certain amount of nutrients (initial_nutrients)
- Every frame the nutrients increase by a certain amount until they reach a maximum (final_nutrients)
- The nutrients of a food are what the protozoa gain when they eat it
*/

struct Food : FoodManagerSettings
{
	int id_ = 0; // unique food ID, relative to the food container
	int body_id_ = 0; // unique body ID, relative to the body container
	float age = 0;
	int time_since_last_reproduced = 0;

	float nutrients = 0.0f;

	sf::Color color{};

	bool active = true;

	void update(Body* body)
	{
		time_since_last_reproduced++;
		age += Random::rand_range(0.4f, 1.0f);

		update_food_nutrients();
		update_food_size(body);

		if (Random::rand01_float() < vibrate_freq)
			vibrate_food(body, vibration_strength);

		//body->velocity_ *= friction;
	}

	bool is_food_dead() const
	{
		return nutrients < initial_nutrients && age > spawn_immunity;
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

	void update_food_size(Body* body)
	{
		// Radius is directly proportional to current nutrients
		if (nutrients == 0.f)
		{
			body->radius_ = 0.f;
			return;
		}

		body->radius_ = nutrients * nutrients_to_radius_scale;
	}

	void vibrate_food(Body* body, float strength)
	{
		body->accelerate(Random::rand_vector(-strength, strength));
	}
};