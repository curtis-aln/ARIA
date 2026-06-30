#include "../food_manager.h"

inline static constexpr float vibrate_freq = 0.0065f;

void FoodManager::vibrate_food(Body* body, float strength)
{
	body->accelerate(Random::rand_vector(-strength, strength));
}


void FoodManager::update_food()
{
	for (Food* food : food_vector)
	{
		Body* body = bodies_->at(food->body_id_);

		food->time_since_last_reproduced++;
		food->age++;

		update_food_size(food, body);

		if (Random::rand01_float() < vibrate_freq)
			vibrate_food(body, vibration_strength);

		body->velocity_ *= friction;

		update_food_nutrients(food);
	}
}



void FoodManager::check_food_death(const Food* food)
{
	// Food dies when its nutrients drop below initial_nutrients (shrunk out of existence)
	if (food->nutrients < initial_nutrients && food->age > 10)
		remove_food(food->id_);
}


void FoodManager::update_food_nutrients(Food* food)
{
	// Nutrients develop from initial_nutrients toward final_nutrients over nutrient_development_time frames.
	// Once the target is reached, no further change is applied.
	// When the food is old enough to die, nutrients start dropping back down instead.

	const bool is_dying = food->age >= death_age;

	if (is_dying)
	{
		// Drain nutrients at the same rate they developed, until hitting initial_nutrients
		const float drain_rate = (final_nutrients - initial_nutrients)
			/ static_cast<float>(nutrient_development_time);
		food->nutrients -= drain_rate;
		return;
	}

	// Normal development toward final_nutrients
	const float diff = final_nutrients - initial_nutrients;
	const float increment = diff / static_cast<float>(nutrient_development_time);

	const bool increasing = diff > 0.f;
	const bool reached_target = increasing
		? food->nutrients >= final_nutrients
		: food->nutrients <= final_nutrients;

	if (reached_target)
		return;

	food->nutrients = std::clamp(
		food->nutrients + increment,
		std::min(initial_nutrients, final_nutrients),
		std::max(initial_nutrients, final_nutrients)
	);
}

void FoodManager::update_food_size(Food* food, Body* body)
{
	// Radius is directly proportional to current nutrients
	if (food->nutrients == 0.f)
	{
		body->radius_ = 0.f;
		return;
	}

	body->radius_ = food->nutrients * nutrients_to_radius_scale;
}

void FoodManager::add_food_to_hash_grid()
{
	// This function adds all the food objects to a seperate hash grd than the body hash grird, the cells will query
	// This hash grid to determine where the food are to handle interactions
	if (frames % update_freq != 0)
		return;

	spatial_hash_grid.clear();
	for (Food* food : food_vector)
	{
		Body* body = bodies_->at(food->body_id_);
		// clamping the body
		const float rad = world_bounds_->bounds_radius;
		sf::Vector2f centre = world_bounds_->center_;
		
		if (!world_bounds_->contains(body->position_))
		{
			sf::Vector2f diff = body->position_ - centre;
			float dist = diff.length();
			sf::Vector2f normal = diff / dist;
			body->position_ = centre + normal * (rad - body->radius_);
		}

		spatial_hash_grid.add_object(body->position_.x, body->position_.y, food->id_);
	}
}

void FoodManager::init()
{
	std::cout << "Initializing food with " << initial_food << " food...\n";

	for (int i = 0; i < initial_food; ++i)
	{
		sf::Vector2f pos = world_bounds_->rand_pos();
		FoodBodyPair food_body_pair = create_food(pos, true);
		if (!food_body_pair.is_valid())
		{
			std::cerr << "Failed to create food at index " << i << ".\n";
			continue;
		}

		Food* food = food_body_pair.food_ptr;
		food->color = Random::rand_color(food_darkest_color, food_lightest_color);
	}

	std::cout << "Initialized " << initial_food << " food.\n";
}
