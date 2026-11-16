#pragma once

#include "../../managers/food_manager/food_manager_settings.h"
#include "../../Utils/random.h"
#include "food_settings.h"
#include <algorithm>
#include <cstdint>
#include <SFML/Graphics/Color.hpp>
#include "../body/body.h"


struct Food : FoodSettings
{
	uint32_t id_ = 0; // unique food ID, relative to the food container
	uint32_t body_id_ = 0; // unique body ID, relative to the body container
	uint16_t age = 0;
	uint16_t time_since_last_reproduced = 0;

	uint16_t fully_grown_age = nutrient_development_time;

	float nutrients = initial_nutrients;

	sf::Color color_inner{};
	sf::Color color_outer{};

	bool active = true;

	float vibration_x = 0.f;
	float vibration_y = 0.f;

	void reset()
	{
		body_id_ = 0;
		age = 0;
		time_since_last_reproduced = 0;
		nutrients = initial_nutrients;

		color_inner = Random::rand_color(food_darkest_color, food_lightest_color);
		color_outer = Random::rand_color(food_darkest_color, food_lightest_color);
		color_inner.a = inner_transparency;
		color_outer.a = outer_transparency;

		active = true;
		vibration_x = 0.f;
		vibration_y = 0.f;
		fully_grown_age = nutrient_development_time;
	}

	void update()
	{
		time_since_last_reproduced++;
		age += 1;
	}

	bool is_food_dead() const
	{
		return nutrients < initial_nutrients && age > spawn_immunity;
	}

	float calculate_food_mass() const
	{
		// Mass is directly proportional to current nutrients
		if (nutrients == 0.f)
			return 0.f;

		return nutrients * nutrients_to_mass_scale;
	}

	bool can_reproduce()
	{
		bool  is_fully_grown = age >= fully_grown_age;
		const bool is_dying = age >= death_age;

		float thresh = final_nutrients * nutrient_reproductive_threshold;
		bool  has_sufficient_nutrients = nutrients >= thresh;
		bool is_cooldown_over = time_since_last_reproduced >= repro_cooldown;

		return is_fully_grown && has_sufficient_nutrients && is_cooldown_over && !is_dying;
	}

	void reproduce(Body* this_body, Food* child, Body* child_body, bool launch_child=false, float launch_strength = 0.f)
	{
		const sf::Vector2f parent_pos = this_body->get_pos();

		// The child should be spawned within a reasonable radius to the parent food
		float r = food_offspring_spawn_distance;
		sf::FloatRect spawn_rect = {{ parent_pos.x - r, parent_pos.y - r }, { r * 2, r * 2 } };
		child_body->set_pos(Random::rand_pos_in_rect(spawn_rect));

		// To spread, the child can sometimes be launched with a random velocity
		if (launch_child)
			child_body->set_vel(Random::rand_vector(-launch_strength, launch_strength));


		time_since_last_reproduced = 0;
		nutrients -= FoodSettings::initial_nutrients;
	}


	static void set_foods_color_transparency(sf::Color& color_to_change,
		const float transparency, const float nutrients, const float age)
	{
		const bool is_dying = age >= death_age;

		if (!is_dying)
		{
			// Fade in over the first kFoodVisibilityRampFrames frames
			const float t = std::min(static_cast<float>(age) / FoodManagerSettings::kFoodVisibilityRampFrames, 1.f);
			color_to_change.a = static_cast<uint8_t>(t * transparency);
		}
		else
		{
			// Fade out as nutrients fall from fade_start_nutrients down to initial_nutrients
			const float range = FoodManagerSettings::fade_start_nutrients - initial_nutrients;
			const float t = std::clamp(
				(nutrients - initial_nutrients) / range,
				0.f, 1.f
			);
			color_to_change.a = static_cast<uint8_t>(t * transparency);
		}
	}

	void vibrate_food(Body* body, int freq, float strength)
	{
		if ((int)age % freq != 0)
			return;

		const uint32_t bits = FastRandom::next_bits();

		// 11 bits each for x and y, mapped to [-1, 1]
		constexpr float inv = 2.f / 2047.f;
		vibration_x = (static_cast<int32_t>((bits >> 11) & 0x7FF) * inv - 1.f) * strength;
		vibration_y = (static_cast<int32_t>(bits & 0x7FF) * inv - 1.f) * strength;

		body->apply_force(sf::Vector2f{ vibration_x, vibration_y });
	}

	void update_food_nutrients()
	{
		// Nutrients develop from initial_nutrients toward final_nutrients over fully_grown_age frames.
		// Once the target is reached, no further change is applied.
		// When the food is old enough to die, nutrients start dropping back down instead.

		const bool is_dying = age >= death_age;

		if (is_dying)
		{
			// Drain nutrients until hitting initial_nutrients
			const float drain_rate = (final_nutrients - initial_nutrients)
				/ static_cast<float>(fully_grown_age);

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
			std::min(0.f, final_nutrients),
			std::max(0.f, final_nutrients));
	}

};