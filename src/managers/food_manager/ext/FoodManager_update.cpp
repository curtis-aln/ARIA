#include "../food_manager.h"



void FoodManager::update_food()
{
	current_total_food_ = food_vector.occupied_count;
	ensure_update_jobs_built();
	thread_pool_.run_and_wait();
}

void FoodManager::update_food_item(Food* food)
{
	Body* body = bodies_->at(food->body_id_);
	food->update();

	body->velocity_ = (body->velocity_ + sf::Vector2f{ food->vibration_x, food->vibration_y }) * friction;
	body->radius_ = food->calculate_food_size();
}


void FoodManager::ensure_update_jobs_built()
{
	if (update_jobs_built_)
		return;

	int updating_threads = WorldSettings::updating_threads;

	updating_bodies_.clear();
	updating_bodies_.reserve(updating_threads);

	// For each of the threads
	for (int t = 0; t < (int)updating_threads; ++t)
	{
		updating_bodies_.emplace_back([this, t, updating_threads] {
			const int total_cells = current_total_food_;
			if (total_cells == 0)
				return;

			const int chunk = std::max(1, (total_cells + (int)updating_threads - 1) / (int)updating_threads);
			const int begin = t * chunk;
			if (begin >= total_cells)
				return;
			const int end = std::min(begin + chunk, total_cells);

			for (int k = begin; k < end; ++k)
			{
				Food* food = food_vector.at(food_vector.occupied_list[k]);
				update_food_item(food);

			}});
	}

	thread_pool_.set_jobs(updating_bodies_);   // only ever called this once
	update_jobs_built_ = true;
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