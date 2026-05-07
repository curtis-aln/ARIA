#include "../food_manager.h"

void FoodManager::spawn_food()
{

}

bool FoodManager::food_container_full() const
{
	return food_vector.size() >= max_food;
}

bool FoodManager::can_food_reproduce(const Food* food)
{
	if (food->time_since_last_reproduced < reproductive_cooldown)
		return false;
	if (food->age < reproductive_threshold)
		return false;
	return true;
}

float FoodManager::calculate_spawn_chance() const
{
	float spawn_chance = 1.f - static_cast<float>(food_vector.size()) / static_cast<float>(max_food);
	return std::clamp(spawn_chance * spawn_proportionality_constant, 0.f, 1.f);
}

void FoodManager::spawn_food_improved()
{
	if (food_container_full())
		return;

	for (Food* food : food_vector)
	{
		if (!can_food_reproduce(food))
			continue;

		if (Random::rand01_float() > calculate_spawn_chance())
			continue;

		if (reproduce_food(food))
			break;
	}
}

bool FoodManager::reproduce_food(Food* food)
{
	Food* new_food = food_vector.add();
	if (new_food == nullptr)
		return true; // max food has been reached

	// spawning the food next to another existing food 
	sf::Vector2f other_food_pos = food->position_;
	sf::FloatRect spawn_rect = {
		{other_food_pos.x - food_spawn_distance, other_food_pos.y - food_spawn_distance},
		{food_spawn_distance * 2, food_spawn_distance * 2}
	};
	new_food->position_ = Random::rand_pos_in_rect(spawn_rect);
	new_food->age = 0;
	new_food->color = Random::rand_color(food_darkest_color, food_lightest_color);

	// small chance of it spawning with a high velocity
	float food_launch_strength = 288.f;
	float food_launch_chance = 0.05f;

	if (Random::rand01_float() < food_launch_chance)
		new_food->velocity_ = Random::rand_vector(-food_launch_strength, food_launch_strength);

	food->time_since_last_reproduced = 0;
	return false;
}