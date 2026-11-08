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

	body->apply_force(sf::Vector2f{ food->vibration_x, food->vibration_y });
	body->mass_ = std::max(food->calculate_food_mass(), 0.1f);
	body->set_radius();

	body->apply_drag(kDragLinearCoeff);
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



void FoodManager::create_food_pool(unsigned amount, WorldBorder* spawn_area)
{
	WorldBorder* spawn_bounds = spawn_area ? spawn_area : world_bounds_;

	for (unsigned i = 0; i < amount; ++i)
	{
		sf::Vector2f pos = spawn_bounds->rand_pos();
		FoodBodyPair food_body_pair = create_food_body_pair(pos);

		Food* food = food_vector.at(food_body_pair.food_id);
		food->age = Random::rand_range(0, age_variation);
		food->fully_grown_age = food->age + Food::nutrient_development_time;
	}
}


void FoodManager::reset_food_manager()
{
	for (Food* food : food_vector)
		bodies_->remove(food->body_id_);
	
	food_vector.clear();
	
	create_food_pool(initial_food);
	initial_iterations();
}


void FoodManager::remove_food_in_area(const sf::Vector2f& center, float radius)
{
	gather_food_in_radius(selected_cells_indexes_, center, radius);

	for (int i = 0; i < selected_cells_indexes_.count; ++i)
		remove_food(food_vector.at(selected_cells_indexes_[i])->id_);
}

void FoodManager::gather_food_in_radius(FixedSpan<cell_idx, uint16_t>& indexes, const sf::Vector2f& position, const float radius)
{
	indexes.clear();

	for (Food* food : food_vector)
	{
		Body* body = bodies_->at(food->body_id_);
		float dist_sq = (body->get_pos() - position).lengthSquared();
		if (dist_sq < radius * radius)
		{
			indexes.add(food->id_);
		}
	}
}


void FoodManager::influence_food_velocities_in_radii(const sf::Vector2f& position, const float radius, const float intensity)
{
	gather_food_in_radius(selected_cells_indexes_, position, radius);

	for (int i = 0; i < selected_cells_indexes_.count; ++i)
	{
		Food* food = food_vector.at(selected_cells_indexes_[i]);
		Body* body = bodies_->at(food->body_id_);

		sf::Vector2f direction = (position - body->get_pos()).normalized();
		body->apply_force(direction * intensity);
	}
}

void FoodManager::update_statistics()
{
	statistics_.food_count = food_vector.size();
}