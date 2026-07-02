#include "../food_manager.h"

struct SimSnapshot;
struct FoodData;

FoodManager::FoodManager(sf::RenderWindow* window, WorldBorder* world_bounds, o_vector<Body>* bodies)
	: world_bounds_(world_bounds), bodies_(bodies), food_renderer(window, food_radius, max_food), window_(window),
	spatial_hash_grid(cells_x, cells_y, cell_max_capacity, world_bounds_->bounds_radius * 2, world_bounds_->bounds_radius * 2), 
	food_grid_renderer(&spatial_hash_grid)
{
	food_data.positions.reserve(max_food);
	food_data.colors.reserve(max_food);
	food_data.radii.reserve(max_food);
	std::cout << "food manager containers resized FoodManager::FoodManager()\n";
}


void FoodManager::update()
{
	add_food_to_hash_grid();
	for (Food* food : food_vector)
	{
		check_food_death(food);
	}

	update_position_data();
	let_food_reproduce();
	update_food();
}

void FoodManager::render(const FoodData& snapshot_food_data)
{
	food_renderer.set_colors(snapshot_food_data.colors);
	food_renderer.set_positions(snapshot_food_data.positions);
	food_renderer.set_radii(snapshot_food_data.radii);

	food_renderer.set_size(snapshot_food_data.active_count);
	food_renderer.update();
	food_renderer.render();
}

void FoodManager::update_position_data()
{
	int food_count = food_vector.size();
	food_data.positions.resize(food_count);
	food_data.radii.resize(food_count);
	food_data.colors.resize(food_count);
	food_data.active_count = food_count;

	int idx = 0;
	for (Food* food : food_vector)
	{
		Body* body = bodies_->at(food->body_id_);
		food_data.positions[idx] = body->position_;
		food_data.radii[idx] = body->radius_;

		sf::Color c = food->color;

		const bool is_dying = food->age >= death_age;

		if (!is_dying)
		{
			// Fade in over the first kFoodVisibilityRampFrames frames
			const float t = std::min(static_cast<float>(food->age) / kFoodVisibilityRampFrames, 1.f);
			c.a = static_cast<uint8_t>(t * kFoodMaxAlpha);
		}
		else
		{
			// Fade out as nutrients fall from fade_start_nutrients down to initial_nutrients
			const float range = fade_start_nutrients - initial_nutrients;
			const float t = std::clamp(
				(food->nutrients - initial_nutrients) / range,
				0.f, 1.f
			);
			c.a = static_cast<uint8_t>(t * kFoodMaxAlpha);
		}

		food_data.colors[idx] = c;
		idx++;
	}
}

// world interacting with the food
void FoodManager::remove_food(const int food_id)
{
	// Fetching the food that we want to remove and using its body_id to find the body we want to remove
	Food* food = food_vector.at(food_id);
	Body* body = bodies_->at(food->body_id_);
	
	// we now reset the body and the food
	body->position_ = { 0, 0 };
	food->age = 0;
	food->time_since_last_reproduced = 0;
	food->nutrients = initial_nutrients;
	
	// and remove them from their respective vectors
	food_vector.remove(food_id);
	bodies_->remove(body->id_);
}

Food* FoodManager::at(const int idx)
{
	return food_vector.at(idx);
}

const Food* FoodManager::at(const int idx) const
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

const o_vector<Food>& FoodManager::get_food_vector() const
{
	return food_vector;
}
o_vector<Food>& FoodManager::get_food_vector()
{
	return food_vector;
}
