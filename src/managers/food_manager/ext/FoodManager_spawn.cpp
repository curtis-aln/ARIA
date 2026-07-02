#include "../food_manager.h"

void FoodManager::let_food_reproduce()
{
	// this function runs a pass over all the food and checks if it can reproduce, if it can then it will spawn a new food 
	// using the reproduction algorithm. new food start off small and with full transparency, they will grow to their full size and color over time.

	// dont do anything if the food container is full
	if (food_container_full())
		return;

	// Each food must pass the reproduce check and the spawn chance
	for (Food* food : food_vector)
	{
		if (!can_food_reproduce(food) || Random::rand01_float() > calculate_spawn_chance())
			continue;

		bool break_out_of_loop = reproduce_food(food);

		if (break_out_of_loop)
			break; // there are no more body vectors left to use
	}
}

float FoodManager::calculate_spawn_chance() const
{
	// This function calculates the chance of a food reproducing based on how much food is in the world
	float spawn_chance = 1.f - static_cast<float>(food_vector.size()) / static_cast<float>(max_food);
	return std::clamp(spawn_chance * spawn_proportionality_constant, 0.f, 1.f);
}


bool FoodManager::reproduce_food(Food* parent_food)
{
	// This function returns true if the food container is full and the food cannot reproduce, false if it can reproduce and has spawned a new food

	if (food_vector.can_add() == false || bodies_->can_add() == false)
		return true;

	Body* parent_body = bodies_->at(parent_food->body_id_);
	sf::Vector2f parent_pos = parent_body->position_;

	// Creating a new food body pair and linking them together
	Food* child_food = food_vector.add();
	Body* child_body = bodies_->add();
	child_food->body_id_ = child_body->id_;

	// spawning the food next to another existing food 
	sf::FloatRect spawn_rect = {
		{parent_pos.x - food_spawn_distance, parent_pos.y - food_spawn_distance},
		{food_spawn_distance * 2, food_spawn_distance * 2}};

	// setting the attributes for this new_body
	child_body->position_ = Random::rand_pos_in_rect(spawn_rect);
	child_body->radius_ = food_initial_radius;
	child_food->age = 0;
	child_food->color = Random::rand_color(food_darkest_color, food_lightest_color);

	// small chance of it spawning with a high velocity
	if (Random::rand01_float() < food_launch_chance)
		child_body->velocity_ = Random::rand_vector(-food_launch_strength, food_launch_strength);

	parent_food->time_since_last_reproduced = 0;
	return false;
}

FoodBodyPair FoodManager::create_food(const sf::Vector2f& position, bool random_genetics)
{
	// This is the safest way to create a food with a body, all creation events Must go through this function to ensure that the food and body are linked correctly.
	// if there are not any already avalable foods in the o_vector we create a new one

	// Finding a body
	Body* body = bodies_->emplace(true, true);
	if (body == nullptr)
		return { nullptr, nullptr };

	// Finding a cell
	Food* food = food_vector.emplace(true, true);
	if (food == nullptr)
	{
		// raise an error as there shouldnt be a situation where we have a body but no cell, this should never happen
		std::cerr << "[ERROR]: Failed to create food during initialization. Max food reached.\n";
		bodies_->remove(body);
		return { nullptr, nullptr };
	}

	// connecting the two
	food->body_id_ = body->id_;
	body->position_ = position;
	body->radius_ = food_initial_radius;

	return { food, body };
}