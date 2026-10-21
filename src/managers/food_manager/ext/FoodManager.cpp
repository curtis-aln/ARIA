#include "../food_manager.h"
#include <cstdint>
#include <entities/body.h>
#include <entities/food/food.h>
#include <entities/food/food_settings.h>
#include <managers/food_manager/food_manager_settings.h>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Vector2.hpp>
#include <simulation/context/sim_command.h>
#include <simulation/context/state.h>
#include <Utils/Graphics/pheromone_grid.h>
#include <Utils/o_vec/o_vector.hpp>
#include <world/world_border.h>
#include <world/world_settings.h>


FoodManager::FoodManager(sf::RenderWindow* window, WorldBorder* world_bounds, o_vector<Body>* bodies)
	: world_bounds_(world_bounds), bodies_(bodies),
	pheromone_grid(2 << pheromone_grid_power, 2 << pheromone_grid_power,
		WorldSettings::bounds_radius * 2.f, WorldSettings::bounds_radius * 2.f)
{

}

void FoodManager::initial_iterations()
{
	constexpr int initial_iterations = 2000;
	for (int i = 0; i < initial_iterations; ++i)
	{
		update(i);
	}
}


void FoodManager::update(int iterations)
{
	if (!food_container_full())
	{
		spawn_random_food();
		spawn_food_mitosis();
	}

	update_food();

	update_statistics();
	handle_food_death(iterations);
}

void FoodManager::handle_food_death(int iterations)
{
	for (Food* food : food_vector)
	{
		if (food->is_food_dead())
			remove_food(food->id_);

		sf::Vector2f pos = bodies_->at(food->body_id_)->position_;

		if (toggles_.update_pheromone_grid)
			pheromone_grid.add_pheromone(pos.x, pos.y, 10.f / pheromone_update_freq);
	}

	if (toggles_.update_pheromone_grid && iterations % pheromone_update_freq == 0)
		pheromone_grid.step();
}

void FoodManager::update_position_data(RenderData& food_data, int iterations)
{
	size_t current_vector_size = food_data.positions.size();
	size_t food_count = food_vector.size();
	size_t reserve_to = current_vector_size + food_count;

	food_data.positions.reserve(reserve_to);
	food_data.radii.reserve(reserve_to);
	food_data.inner_colors.reserve(reserve_to);
	food_data.outer_colors.reserve(reserve_to);
	food_data.velocities.reserve(reserve_to);

	if (toggles_.render_pheromone_grid)
	{
		if (iterations % pheromone_update_freq == 0)
		{
			pheromone_grid.render();
		}
		food_data.pheromone_texture = pheromone_grid.get_texture();
	}

	for (Food* food : food_vector)
	{
		Body* body = bodies_->at(food->body_id_);

		food_data.positions.push_back(body->position_);
		food_data.radii.push_back(body->radius_);
		food_data.velocities.push_back(body->velocity_);

		// handling color
		sf::Color inner_col_copy = food->color_inner;
		sf::Color outer_col_copy = food->color_outer;
		Food::set_foods_color_transparency(inner_col_copy, food->inner_transparency, food->nutrients, food->age);
		Food::set_foods_color_transparency(outer_col_copy, food->outer_transparency, food->nutrients, food->age);
		food_data.inner_colors.push_back(inner_col_copy);
		food_data.outer_colors.push_back(outer_col_copy);

	}
}


// world interacting with the food
void FoodManager::remove_food(const int food_id)
{
	// Fetching the food that we want to remove and using its body_id to find the body we want to remove
	Food* food = food_vector.at(food_id);
	Body* body = bodies_->at(food->body_id_);

	// we now reset the body and the food
	body->reset_cell_manager();
	food->reset();

	// and remove them from their respective vectors
	food_vector.remove(food_id);
	bodies_->remove(body->id_);
}

bool FoodManager::has_food_with_body_id(int body_id) const
{
	for (Food* food : food_vector)
	{
		if (food->body_id_ == body_id)
			return true;
	}
	return false;
}

void FoodManager::handle_food_manager_event(SimCommand& cmd)
{
	switch (cmd.type)
	{
	case CommandType::SetFoodToggles:
		toggles_ = cmd.food_toggles;
		break;

	case CommandType::SetRandomSpawnsPerFrame:
		FoodManagerSettings::food_random_spawn_per_frame = cmd.int_val;
		break;

	case CommandType::SetRandomSpawnChance:
		FoodManagerSettings::food_random_spawn_chance = cmd.float_val;
		break;

	case CommandType::SetMitosisConstant:
		FoodManagerSettings::spawn_proportionality_constant = cmd.float_val;
		break;

	case CommandType::SetPheromoneUpdatingSteps:
		PheromoneGridSettings::substeps = cmd.int_val;
		break;

	case CommandType::SetPheromoneDecayRate:
		PheromoneGridSettings::decay_rate = cmd.float_val;
		break;

	case CommandType::SetPheromoneDiffusionRate:
		PheromoneGridSettings::diffuse_rate = cmd.float_val;
		break;

	case CommandType::SetPheromoneDepositAmount:
		PheromoneGridSettings::deposit_amount = cmd.float_val;
		break;

	case CommandType::SetPheromoneMaxPheromone:
		PheromoneGridSettings::max_pheromone = cmd.float_val;
		break;

	case CommandType::SetFoodNutrients:
		FoodSettings::final_nutrients = cmd.float_val;
		break;

	case  CommandType::SetFoodNutrientsDevelopmentTime:
		FoodSettings::nutrient_development_time = static_cast<uint16_t>(cmd.int_val);
		break;

	case  CommandType::SetFoodFriction:
		FoodManagerSettings::friction = cmd.float_val;
		break;

	case  CommandType::SetFoodVibrationStrength:
		FoodSettings::vibration_strength = cmd.float_val;
		break;

	case CommandType::SetFoodReproductiveCooldown:
		FoodSettings::repro_cooldown = static_cast<size_t>(cmd.float_val);
		break;

	case  CommandType::SetFoodDeathAge:
		FoodSettings::death_age = cmd.float_val;
		break;

	case  CommandType::SetFoodReproductiveThreshold:
		FoodSettings::nutrient_reproductive_threshold = cmd.float_val;
		break;

	case CommandType::SetPheromoneUpdateFrequency:
		FoodManagerSettings::pheromone_update_freq = cmd.int_val;
		break;

	case  CommandType::SetPheromoneRenderFrequency:
		FoodManagerSettings::pheromone_render_update_freq = cmd.int_val;
		break;
	}
}