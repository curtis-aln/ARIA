#include "food_manager.h"

struct SimSnapshot;
struct FoodData;

FoodManager::FoodManager(sf::RenderWindow* window, Circle* world_circular_bounds)
	: world_bounds_(world_circular_bounds), food_renderer(window, food_radius, max_food), window_(window)
{
	init_food();

	food_data.positions_x.resize(max_food, {});
	food_data.positions_y.resize(max_food, {});
	food_data.colors.resize(max_food, {});
	food_data.radii.resize(max_food, food_radius);
}

void FoodManager::update()
{

	food_data.active_count = food_vector.size();
	update_position_data();
	spawn_food_improved(); //spawn_food();
	update_food();
	update_hash_grid();
}

void FoodManager::render(const SimSnapshot& snapshot)
{
	food_renderer.set_colors(snapshot.food_data.colors);
	food_renderer.set_positions_x(snapshot.food_data.positions_x);
	food_renderer.set_positions_y(snapshot.food_data.positions_y);
	food_renderer.set_radii(snapshot.food_data.radii);

	food_renderer.set_size(snapshot.food_data.active_count);
	food_renderer.update();
	food_renderer.render();
}

void FoodManager::update_position_data()
{
	int idx = 0;
	for (Food* food : food_vector)
	{
		food_data.positions_x[idx] = food->position.x;
		food_data.positions_y[idx] = food->position.y;

		sf::Color c = food->color;

		float age = static_cast<float>(food->age);
		c.a = std::min(age / kFoodVisibilityRampFrames, 1.f) * kFoodMaxAlpha;

		food_data.colors[idx] = c;
		idx++;
	}
}

// world interacting with the food
void FoodManager::remove_food(const int food_id)
{
	Food* food = food_vector.at(food_id);
	food->position = { 0, 0 };
	food->age = 0;
	food->time_since_last_reproduced = 0;
	food->nutrients = initial_nutrients;
	food_vector.remove(food_id);
}

Food* FoodManager::at(const int idx)
{
	return food_vector.at(idx);
}

void FoodManager::draw_food_grid(sf::Vector2f mouse_pos) const
{
	food_grid_renderer.render(*window_, mouse_pos, 1600.f);
}

int FoodManager::get_size() const
{
	return food_vector.size();
}


void FoodManager::update_food_grid_renderer()
{
	food_grid_renderer.rebuild();
}

void FoodManager::fill_data(FoodData& other_food_data)
{
	other_food_data = food_data;
}