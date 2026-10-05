#include "../food_manager.h"
#include <algorithm>
#include <cstdint>
#include <entities/body.h>
#include <entities/food/food.h>
#include <iostream>
#include <managers/food_manager/food_manager_settings.h>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>
#include <Utils/random.h>


void FoodManager::spawn_random_food()
{
	if (!toggles_.spawn_random_food)
		return;

	for (int i = 0; i < FoodManagerSettings::food_random_spawn_per_frame; ++i)
	{
		if (Random::rand01_float() < FoodManagerSettings::food_random_spawn_chance)
			create_food_pool(1, world_bounds_);
	}
}

void FoodManager::spawn_food_mitosis()
{
	if (!toggles_.food_mitosis)
		return;

	float spawn_chance = calculate_spawn_chance();

	// Each food must pass the reproduce check and the spawn chance
	for (Food* food : food_vector)
		if (food->can_reproduce() && Random::rand01_float() < spawn_chance)
			reproduce_food(food);
}


float FoodManager::calculate_spawn_chance() const
{
	// This function calculates the chance of a food reproducing based on how much food is in the world
	float ratio = static_cast<float>(food_vector.size()) / static_cast<float>(max_food);
	float spawn_chance = 1.f - ratio;
	float scaled = spawn_chance * spawn_proportionality_constant;
	return std::clamp(scaled, 0.f, 1.f);
}


void FoodManager::reproduce_food(Food* parent_food)
{
	// This function returns true if the food container is full and the food cannot reproduce, false if it can reproduce and has spawned a new food
	Body* parent_body = bodies_->at(parent_food->body_id_);
	sf::Vector2f parent_pos = parent_body->position_;

	// Creating a new food body pair and linking them together
	FoodBodyPair pair = create_food_body_pair(parent_pos);
	if (pair.is_valid() == false)
		return;

	Food* child_food = food_vector.at(pair.food_id);
	Body* child_body = bodies_->at(pair.body_id);

	// spawning the food next to another existing food 
	sf::FloatRect spawn_rect = {
		{ parent_pos.x - food_spawn_distance, parent_pos.y - food_spawn_distance },
		{ food_spawn_distance * 2, food_spawn_distance * 2 } };

	// setting the attributes for this new_body
	child_body->position_ = Random::rand_pos_in_rect(spawn_rect);

	// small chance of it spawning with a high velocity
	if (Random::rand01_float() < food_launch_chance)
		child_body->velocity_ = Random::rand_vector(-food_launch_strength, food_launch_strength);

	parent_food->reproduce();
	return;
}


FoodBodyPair FoodManager::create_food_body_pair(const sf::Vector2f& position)
{
	// This is the safest way to create a food with a body, all creation events Must go through this function to ensure that the food and body are linked correctly.
	// if there are not any already avalable foods in the o_vector we create a new one

	// Finding a body
	Body* body = bodies_->emplace(true, true);
	if (body == nullptr)
		return { -1, -1 };

	// Finding a cell
	Food* food = food_vector.emplace(true, true);
	if (food == nullptr)
	{
		// raise an error as there shouldnt be a situation where we have a body but no cell, this should never happen
		std::cerr << "[ERROR]: Failed to create food during initialization. Max food reached.\n";
		bodies_->remove(body);
		return { -1, -1 };
	}

	// connecting the two
	food->reset();
	food->body_id_ = body->id_;
	body->position_ = position;

	return { (int32_t)food->id_, (int32_t)body->id_ };
}