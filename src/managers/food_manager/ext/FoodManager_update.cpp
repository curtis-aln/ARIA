#include "../food_manager.h"



void FoodManager::update_food()
{
	for (Food* food : food_vector)
	{
		if (food->is_food_dead())
			remove_food(food->id_);

		Body* body = bodies_->at(food->body_id_);
		food->update();

		body->velocity_ = (body->velocity_ + sf::Vector2f{ food->vibration_x, food->vibration_y }) * friction;
		body->radius_ = food->calculate_food_size();
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
		food->nutrients = Random::rand_range(initial_nutrients, initial_nutrients + 50.f);
	}

	std::cout << "Initialized " << initial_food << " food.\n";
}


void FoodManager::reset()
{
	for (Food* food : food_vector)
	{
		bodies_->remove(food->body_id_);
		food_vector.remove(food);
	}

	init();
}


void FoodManager::remove_food_in_area(const sf::Vector2f& center, float radius)
{
	gather_food_in_radius(select_indexes, center, radius);

	for (int i = 0; i < select_indexes.count; ++i)
		remove_food(food_vector.at(select_indexes[i])->id_);
}

void FoodManager::gather_food_in_radius(FixedSpan<cell_idx, uint16_t>& indexes, const sf::Vector2f& position, const float radius)
{
	indexes.clear();

	for (Food* food : food_vector)
	{
		Body* body = bodies_->at(food->body_id_);
		float dist_sq = (body->position_ - position).lengthSquared();
		if (dist_sq < radius * radius)
		{
			indexes.add(food->id_);
		}
	}
}


void FoodManager::influence_food_velocities_in_radii(const sf::Vector2f& position, const float radius, const int intensity)
{
	gather_food_in_radius(select_indexes, position, radius);

	for (int i = 0; i < select_indexes.count; ++i)
	{
		Food* food = food_vector.at(select_indexes[i]);
		Body* body = bodies_->at(food->body_id_);

		sf::Vector2f direction = (position - body->position_).normalized();
		body->velocity_ += direction * (float)intensity;
	}
}

void FoodManager::update_statistics()
{
	statistics_.food_count = food_vector.size();
}