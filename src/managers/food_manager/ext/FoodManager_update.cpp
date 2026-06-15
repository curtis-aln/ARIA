#include "../food_manager.h"

void FoodManager::vibrate_food(Food* food, float strength)
{
	Body* body = bodies_->at(food->body_id_);
	body->velocity_ += Random::rand_vector(-strength, strength);
}


void FoodManager::update_food()
{
	for (Food* food : food_vector)
	{
		Body* body = bodies_->at(food->body_id_);

		food->time_since_last_reproduced++;
		food->age++;

		constexpr float vibrate_freq = 0.1f;

		vibrate_food(food, vibration_strength * Random::rand01_float() < vibrate_freq);

		body->velocity_ *= friction;
		body->position_ += body->velocity_;

		bound_food_to_world(food);

		update_food_nutrients(food);
		check_food_death(food);
	}
}

void FoodManager::bound_food_to_world(Food* food) const
{
	Body* body = bodies_->at(food->body_id_);

	sf::Vector2f center = world_bounds_->center_;
	const float radius = world_bounds_->bounds_radius;
	const float dist_sq = (body->position_ - center).lengthSquared();

	const float max_dist = radius - food_radius;

	if (dist_sq < max_dist * max_dist)
		return;

	const sf::Vector2f to_center = center - body->position_;
	const sf::Vector2f normal = to_center.normalized();
	body->position_ = center + normal * max_dist;
}

void FoodManager::check_food_death(const Food* food)
{
	if (food->age < death_age)
		return;

	if (Random::rand01_float() < death_age_chance)
		remove_food(food->id_);
}


void FoodManager::update_food_nutrients(Food* food)
{
	
	const bool is_max = food->nutrients > final_nutrients;
	const float diff = final_nutrients - initial_nutrients;
	

	const float increment = (diff / nutrient_development_time) * !is_max;
	food->nutrients += increment;


}

void FoodManager::update_hash_grid()
{
	if (frames % update_freq != 0)
		return;

	spatial_hash_grid.clear();
	for (Food* food : food_vector)
	{
		Body* body = bodies_->at(food->body_id_);
		spatial_hash_grid.add_object(body->position_.x, body->position_.y, food->body_id_);
	}
}

void FoodManager::init()
{
	std::cout << "Initializing food with " << initial_food << " food...\n";

	bool is_object_active = true; // the first few objects will be active, the rest will be inactive

	for (int i = 0; i < max_food; ++i)
	{
		Food* food = food_vector.emplace(is_object_active);
		food->color = Random::rand_color(food_darkest_color, food_lightest_color);

		if (!link_food_to_body(food, is_object_active))
		{
			std::cerr << "[ERROR]: Failed to link food to body during initialization. Max bodies reached.\n";
			break;
		}
		if (i >= initial_food)
		{
			is_object_active = false; // the rest of the objects will be inactive
		}
	}

	std::cout << "Initialized " << initial_food << " food.\n";
}

bool FoodManager::link_food_to_body(Food* food, bool is_active)
{
	// This function creates a new body for the food and links them together
	// returns true if successful, false if there are no more bodies available
	Body* body = bodies_->emplace(is_active);
	if (body == nullptr)
	{
		return false;
	}
	food->body_id_ = body->id_;

	// Set the initial position of the food to a random location within the world bounds
	body->position_ = world_bounds_->rand_pos();

	return true;
}