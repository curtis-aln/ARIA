#pragma once


#include "../../entities/body/body.h"
#include "../../entities/food/food.h"

#include "../../simulation/context/state.h"
#include "../../Utils/o_vec/o_vector.hpp"
#include "../../Utils/spatial_grid/simple_spatial_grid.h"
#include "food_manager_settings.h"
#include "world/world_border.h"
#include <Utils/thread_pool.h>

#include <simulation/context/sim_command.h>
#include <world/world_settings.h>

#include "../../Utils/Graphics/pheromone_grid.h"
#include <cstdint>
#include <functional>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Vector2.hpp>
#include <Utils/spatial_grid/fixed_span.h>
#include <vector>


struct FoodBodyPair
{
	int32_t food_id;
	int32_t body_id;

	bool is_valid() const
	{
		return (food_id != -1) && (body_id != -1);
	}
};

inline static constexpr int age_variation = 1500; // random age variation for food


struct SimSnapshot;

class FoodManager : public FoodManagerSettings
{
	// pointers to useful data from the world
	WorldBorder* world_bounds_;
	o_vector<Body>* bodies_;

	// the food container, this is where all the food is stored
	o_vector<Food> food_vector{ max_food };

	// Multithreadding the update of the food
	BarrierThreadPool thread_pool_{ (int)WorldSettings::updating_threads };
	std::vector<std::function<void()>> updating_bodies_;
	bool update_jobs_built_ = false;
	unsigned  current_total_food_ = 0;

	// pheromone grid
	PheromoneGrid pheromone_grid;

	FoodManagerStatistics statistics_{};

public:
	FixedSpan<cell_idx, uint16_t> selected_cells_indexes_{ static_cast<uint16_t>(10000) };

	FoodToggles toggles_{};

public:
	FoodManager(sf::RenderWindow* window, WorldBorder* world_bounds, o_vector<Body>* bodies);

	void initial_iterations();

	// Main Entries
	void update(int iterations);
	void update_position_data(RenderData& food_data, int iterations);
	void handle_food_manager_event(SimCommand& cmd);
	void reset_food_manager();

	// Food Manipulation
	void create_food_pool(unsigned amount, WorldBorder* spawn_area = nullptr);
	void remove_food(int food_id);

	// Mouse Influence
	void remove_food_in_area(const sf::Vector2f& center, float radius);
	void gather_food_in_radius(FixedSpan<cell_idx, uint16_t>& indexes, const sf::Vector2f& position, const float radius);
	void influence_food_velocities_in_radii(const sf::Vector2f& position, const float radius, const float intensity);

	// Information Fetching
	[[nodiscard]] int get_size() const { return food_vector.size(); }
	[[nodiscard]] const o_vector<Food>& get_food_vector() const { return food_vector; }
	[[nodiscard]] o_vector<Food>& get_food_vector() { return food_vector; }
	[[nodiscard]] FoodManagerStatistics& get_statistics() { return statistics_; }
	[[nodiscard]] PheromoneGrid* get_pheromone_grid() { return &pheromone_grid; }
	[[nodiscard]] Food* at(int idx) { return food_vector.at(idx); }
	[[nodiscard]] const Food* at(int idx) const { return food_vector.at(idx); }
	[[nodiscard]] bool food_container_full() { return food_vector.size() >= max_food; }

	[[nodiscard]] bool has_food_with_body_id(int body_id) const;


private:
	// Private Utility
	[[nodiscard]] FoodBodyPair create_food_body_pair(const sf::Vector2f& position);

	// Updating
	void  update_food();
	void update_food_item(Food* food);

	void ensure_update_jobs_built();

	// Statistics
	void update_statistics();

	// Reproduction
	void spawn_random_food();
	void spawn_food_mitosis();
	void  reproduce_food(Food* food);
	float calculate_spawn_chance() const;

	// Death
	void handle_food_death(int iterations);
};
