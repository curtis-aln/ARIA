#include "../food_manager.h"

void FoodManager::vibrate_food(Food* food, float strength)
{
	Body* body = bodies_->at(food->id_);
	body->velocity_ += Random::rand_vector(-strength, strength);
}


void FoodManager::update_food()
{
	for (Food* food : food_vector)
	{
		Body* body = bodies_->at(food->id_);

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
	Body* body = bodies_->at(food->id_);

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
		Body* body = bodies_->at(food->id_);
		spatial_hash_grid.add_object(body->position_.x, body->position_.y, food->id_);
	}
}

void FoodManager::init()
{
	std::cout << "Initializing food with " << initial_food << " food...\n";
	for (int i = 0; i < max_food; ++i)
	{
		// Creating the body
		int id = bodies_->emplace({});
		Body* body = bodies_->at(id);
		body->id_ = id;

		body->position_ = Random::rand_position_in_circle(world_bounds_->center_, world_bounds_->bounds_radius);
		body->velocity_ = Random::rand_vector(-10.f, 10.f);		

		// Creating the food and connecting it to the body
		Food food{};
		food.id_ = body->id_;
		
		// Setting food attributes
		food.color = Random::rand_color(food_darkest_color, food_lightest_color);
		food_vector.emplace(food);
	}

	int i = 0;
	int to_remove = max_food - initial_food;
	for (Food* food : food_vector)
	{
		if (i++ >= to_remove)
		{
			break;
		}

		Body* body = bodies_->at(food->id_);

		food_vector.remove(food);
		bodies_->remove(body);
	}
	std::cout << "Initialized " << initial_food << " food.\n";
}