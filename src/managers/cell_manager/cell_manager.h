#pragma once
#include <algorithm>
#include <iostream>

#include "../../Utils/o_vec/o_vector.hpp"
#include <SFML/Graphics.hpp>

#include "cell_manager_settings.h"

#include "../../entities/cell/cell.h"
#include "../../entities/spring/spring.h"
#include "world/world_border.h"

#include "../food_manager/food_manager.h"
#include "organism_tracker.h"
#include "../../simulation/context/sim_snapshot.h"

struct CellBodyPair
{
	Cell* cell_ptr;
	Body* body_ptr;

	bool is_valid()
	{
		return (cell_ptr != nullptr) && (body_ptr != nullptr);
	}
};


/* How reproduction works, in detail
when a cell has enough energy its sets reproduce = true
the protozoa manager detects this and
- cell.reproduce = false;
- makes a create cell request

The birth manager then processes this request by
- creating a new cell
- cell.offspring_index = the index of the new cell
- a temporary spring is made between the parent cell and the new cell

This keeps happening until a spring detects that both its cell's have a valid offspring index
The spring then
- sets cell_a.connection_index = cell_b.offspring_index; 
- cell_a.spring_to_copy_index = id; this is the spring data that will be used
This tells the protozoa manager what to connect (cell_a.connection_index, cell_b.offspring_index)

The connection request is detected
 */


struct BirthRequest
{
	uint32_t parent_cell_id;
};

struct ConnectionRequest
{
	int32_t offspring_id;
	int32_t connect_to_id;
	int32_t  spring_to_copy_index;

	bool weak = false;
};

inline static constexpr size_t max_lifetime_samples_ = 500;
inline static constexpr int survival_rate_window_size_ = 100;


// A Class which handles all protozoa related stuff in the world. updating, collisions, reproduction, etc.
class CellManager: protected CellManagerSettings
{
	sf::RenderWindow* m_window_ = nullptr;
	WorldBorder* world_bounds_ = nullptr;

	uint8_t total_max_capacity = FoodManagerSettings::cell_max_capacity * static_cast<uint8_t>(9);
	FixedSpan<obj_idx> nearby_food_ids{ total_max_capacity };

	// The user can click on a protozoa to select it for debugging purposes. we store a pointer to it here.
	Cell* selected_cell = nullptr;

	std::vector<float> recent_lifetimes_ = {}; // a vector storing the lifetimes of the 500 most recent protozoa deaths, used to calculate average_lifetime_
	std::vector<float> distribution_{}; // a vector storing the generation of all protozoa in the world, used to calculate average generation

	// used to store requests for new protozoa to be created, and for new connections to be made between cells
	std::vector<BirthRequest> birth_requests;
	std::vector<ConnectionRequest> connection_requests;
	CellManagerStatistics statistics_{};

	// main body class is kept in the world class, we keep a pointer to it here so we can access it
	o_vector<Body>* bodies_;

	// the main data structure for storing all protozoa in the world, this is a custom vector class that allows for fast iteration over active objects
	o_vector<Cell> all_cells_;
	o_vector<Spring> all_springs_;

	// This builds a model around a protozoa that doesnt globally exist, so it can be monitored and learned about
	OrganismTracker protozoa_tracker_{};

	

public:
	uint16_t max_size = static_cast<uint16_t>(10000);
	FixedSpan<cell_idx, uint16_t> select_indexes{ max_size };	

	// Functions
public:
	// Constructor and initialization
	CellManager(sf::RenderWindow* window, WorldBorder* world_bounds, o_vector<Body>* bodies);

	void reset();

	bool has_cell_with_body_id(int body_id);
	
	void create_new_protozoa(int count, WorldBorder* spawn_area);
	void check_for_extinction_event();
	void drag_selected_cell_to_point(const sf::Vector2f& target_position, const float move_fraction);

	CellBodyPair create_cell(const sf::Vector2f& position, bool random_genetics = false);

	Spring* create_spring(const int cell_a_id, const int cell_b_id);

	void gather_food_in_radius(FixedSpan<cell_idx, uint16_t>& indexes, const sf::Vector2f& position, const float radius);

	void remove_cells_in_radius(const sf::Vector2f& position, const float radius);

	void influence_cell_velocities_in_radii(const sf::Vector2f& position, const float radius, const int intensity);

	// data fetching
	int get_cell_count() const;
	float calculate_average_generation() const;
	Cell* find_cell_by_id(const int id) { return all_cells_.at(id); }
	Cell* find_cell_at_point(const sf::Vector2f mouse_position, bool make_selected_cell);
	void fill_snapshot(SimSnapshot& snapshot, sf::FloatRect& visible_bounds);
	void fill_render_data(RenderData& render_data, sf::FloatRect& visible_bounds);
	const sf::Vector2f* get_selected_protozoa_pos() const;

	sf::Vector2f& get_cell_pos(int cell_id);

	Body* get_cell_body(int cell_id);

	const o_vector<Cell>& get_all_cells() const { return all_cells_; }
	o_vector<Cell>& get_all_cells() { return all_cells_; }
	const o_vector<Spring>& get_all_springs() const { return all_springs_; }
	o_vector<Spring>& get_all_springs() { return all_springs_; }

	// selected cell management
	void deselect_cell();
	Cell* get_selected_cell() const { return selected_cell; }
	
	// updating, public
	void update();
	void update_food_interactions(FoodManager& food_manager);

	// public statistics
	const std::vector<float>& get_generation_distribution();

	void update_statistics();
	CellManagerStatistics& get_statistics() { return statistics_; }

protected: // only functions the world can access todo


private: // only functions this class can access
	// protozoa building, and reproducing
	bool build_protozoa_from_seed(Cell* seed_cell, int max_recursion_depth, int recursion_depth = 1);
	void collect_reproduction_requests();
	void apply_birth_requests();
	void apply_connection_requests();

	// statistics 
	void register_death_stat(const float lifetime, const bool had_offspring);
	void register_birth_stat();

	// updating
	void update_springs();
	void update_cells();

	void handle_death();
	void remove_cell(Cell* cell);
};