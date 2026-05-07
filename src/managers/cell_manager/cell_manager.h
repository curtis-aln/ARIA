#pragma once
#include <algorithm>
#include <iostream>

#include "../../Utils/o_vector.hpp"
#include <SFML/Graphics.hpp>

#include "cell_manager_settings.h"

#include "../../entities/cell/cell.h"
#include "../../entities/spring/spring.h"
#include "world/world_border.h"


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
	uint32_t offspring_id;
	uint32_t connect_to_id;
	int32_t  spring_to_copy_index;
};


// A Class which handles all protozoa related stuff in the world. updating, collisions, reproduction, etc.
class CellManager: protected CellManagerSettings
{
public:
	sf::RenderWindow* m_window_ = nullptr;

	uint16_t   longest_lived_ever_ = 0;
	uint8_t   most_offspring_ever_ = 0;
	float infant_mortality_rate_ = 0.f;
	int   total_deaths_ = 0;
	int   infant_deaths_ = 0;

	WorldBorder* world_bounds_ = nullptr;

public:
	// The user can click on a protozoa to select it for debugging purposes. we store a pointer to it here.
	Cell* selected_cell = nullptr;
	

	float average_lifetime_ = 0.f; // the average lifetime of the 500 most recent protozoa deaths

	std::vector<float> recent_lifetimes_ = {}; // a vector storing the lifetimes of the 500 most recent protozoa deaths, used to calculate average_lifetime_

	static constexpr size_t max_lifetime_samples_ = 500;

	int deaths_this_window_ = 0;
	int births_this_window_ = 0;
	static constexpr int survival_rate_window_size_ = 100;

	std::vector<BirthRequest> birth_requests;
	std::vector<ConnectionRequest> connection_requests;

	o_vector<Cell> all_cells_;
	o_vector<Spring> all_springs_;

	// every frame this is filled with the collision resolutions calculated in the collision detection phase, and then applied to the cells in the update phase. 
	// this is done to avoid modifying cell velocities during the collision detection phase which can cause errors in subsequent collision checks within the same frame.
	alignas(64) std::vector<sf::Vector2f> collision_resolutions{};


	CellManager(sf::RenderWindow* window, WorldBorder* world_bounds);

	Cell* get_selected_cell() const { return selected_cell; }

	int get_protozoa_count() const;

	float calculate_average_generation() const;

	void deselect_cell();


	Cell* find_cell_by_id(const int id) { return all_cells_.at(id);}


	void build_protozoa();

	void update(bool update_physics_only = false);

	void update_cell_collisions() const;
	void check_for_extinction_event();
	void bound_cell(Cell* cell);

	void register_death_stat(const float lifetime, const bool had_offspring);

	void register_birth_stat();

	void init_protozoa_container();

private:
	void collect_reproduction_requests(std::vector<Cell*>& cells);
	void apply_birth_requests(std::vector<Cell>& cells, std::vector<Spring>& springs);
	void apply_connection_requests(std::vector<Cell>& cells, std::vector<Spring>& springs);
};