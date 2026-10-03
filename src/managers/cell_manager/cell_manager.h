#pragma once

#include "../../Utils/o_vec/o_vector.hpp"

#include "cell_manager_settings.h"

#include "../../entities/cell/cell.h"
#include "../../entities/spring/spring.h"
#include "world/world_border.h"

#include "../../simulation/context/sim_snapshot.h"
#include "organism_tracker.h"

#include <cstdint>
#include <deque>
#include <entities/body.h>
#include <entities/matter/CellMatter.h>
#include <functional>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Vector2.hpp>
#include <simulation/context/sim_command.h>
#include <simulation/context/state.h>
#include <Utils/spatial_grid/fixed_span.h>
#include <Utils/spatial_grid/simple_spatial_grid.h>
#include <Utils/thread_pool.h>
#include <vector>
#include <world/world_settings.h>


struct CellBodyPair
{
	uint32_t cell_id{};
	uint32_t body_id{};

	bool is_valid{};
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

struct MatterBirthRequest
{
	sf::Vector2f position;
	sf::Color inner_color;
	sf::Color  outer_color;
};

struct ConnectionRequest
{
	int32_t offspring_id;
	int32_t connect_to_id;
};

inline static constexpr uint16_t max_mouse_selection_count = static_cast<uint16_t>(10000);

// A Class which handles all protozoa related stuff in the world. updating, collisions, reproduction, etc.
class CellManager : protected CellManagerSettings
{
	sf::RenderWindow* m_window_ = nullptr;
	WorldBorder* world_bounds_ = nullptr;

	uint8_t total_max_capacity = WorldSettings::collision_max_capacity * static_cast<uint8_t>(9);
	FixedSpan<obj_idx> nearby_food_ids{ total_max_capacity };
	FixedSpan<obj_idx> nearby_cell_ids{ total_max_capacity };

	// The user can click on a protozoa to select it for debugging purposes. we store a pointer to it here.
	int32_t selected_cell_id_ = -1;

	// header
	std::deque<float> recent_lifetimes_;
	float recent_lifetimes_sum_ = 0.f; // a vector storing the lifetimes of the 500 most recent protozoa deaths, used to calculate average_lifetime_

	std::vector<float> distribution_{}; // a vector storing the generation of all protozoa in the world, used to calculate average generation

	// used to store requests for new protozoa to be created, and for new connections to be made between cells
	std::vector<BirthRequest> cell_birth_requests;
	std::vector<MatterBirthRequest> matter_birth_requests;
	std::vector<ConnectionRequest> connection_requests;
	std::vector<uint32_t> spring_death_requests;
	CellManagerStatistics statistics_{};

	// death requests stop us from removing cells while we are iterating over them, we store the ids of the cells to be removed here and remove them after the iteration is done
	std::vector<cell_idx> cell_death_requests_;
	std::vector<cell_idx> matter_death_requests_;

	// main body class is kept in the world class, we keep a pointer to it here so we can access it
	o_vector<Body>* bodies_;

	// the main data structure for storing all protozoa in the world, this is a custom vector class that allows for fast iteration over active objects
	o_vector<Cell> all_cells_;
	o_vector<Spring> all_springs_;
	o_vector<CellMatter> all_cell_matter_;

	// This builds a model around a protozoa that doesnt globally exist, so it can be monitored and learned about
	OrganismTracker protozoa_tracker_{};

	std::vector<std::function<void()>> updating_bodies_;
	BarrierThreadPool thread_pool_{ (int)WorldSettings::updating_threads };
	bool update_jobs_built_ = false;
	int  current_total_cells_ = 0;

	// this spatial grid holds new born cells so that the they can form connections between other newly born cells
	uint32_t cells_x = 2 << WorldSettings::birth_cell_power;
	SimpleSpatialGrid new_born_cell_grid_{ cells_x, cells_x, WorldSettings::birth_max_capacity, WorldSettings::bounds_radius * 2.0f, WorldSettings::bounds_radius * 2.0f };

	bool spawn_immune = false;

	// easy reproduction
	static constexpr int max_protozoa_cells = 10;
	FixedSpan<cell_idx> protozoa_cell_ids_{ max_protozoa_cells };

public:
	CellManagerToggles toggles_{};

	// These two are used for when the mouse wants to manipulate cells and matter inside of a certain radius
	FixedSpan<cell_idx, uint16_t> selected_cells_indexes_{ max_mouse_selection_count };
	FixedSpan<cell_idx, uint16_t> selected_matter_indexes_{ max_mouse_selection_count };

	bool extinction_event = false;

public:
	// Constructor
	CellManager(sf::RenderWindow* window, WorldBorder* world_bounds, o_vector<Body>* bodies);
	void reset_cell_manager();

	// entries
	void handle_cell_manager_event(SimCommand& cmd);
	void mutate_selected_protozoa();
	void update(int iterations);
	void update_position_container(RenderData& rend_data, const sf::FloatRect& visible_bounds, const bool show_only_newborns);
	void update_protozoa_tracker();
	void fill_snapshot(SimSnapshot& snapshot, sf::FloatRect& visible_bounds) const;

	// user input & mouse handling
	void create_new_protozoa(int count, WorldBorder* spawn_area);
	void drag_selected_cell_to_point(const sf::Vector2f& target_position, const float move_fraction);
	void remove_cells_in_radius(const sf::Vector2f& position, const float radius);
	void influence_cell_velocities_in_radii(const sf::Vector2f& position, const float radius, const float intensity);
	void kill_selected_protozoa();
	void force_reproduce_selected_protozoa();

	// data fetching
	unsigned get_cell_count() const { return all_cells_.size(); }
	unsigned get_matter_count() const { return all_cell_matter_.size(); }

	bool has_cell_with_body_id(int body_id);

	Cell* find_cell_by_id(const int id) { return all_cells_.at(id); }
	Cell* find_cell_at_point(const sf::Vector2f mouse_position, bool make_selected_cell);
	sf::Vector2f& get_cell_pos(int cell_id);
	Body* get_cell_body(int cell_id);

	const sf::Vector2f* get_selected_protozoa_pos() const;

	const o_vector<Cell>& get_all_cells() const { return all_cells_; }
	const o_vector<Spring>& get_all_springs() const { return all_springs_; }
	const o_vector<CellMatter>& get_all_cell_matter() const { return all_cell_matter_; }

	o_vector<Cell>& get_all_cells() { return all_cells_; }
	o_vector<Spring>& get_all_springs() { return all_springs_; }

	SimpleSpatialGrid* get_new_born_cell_grid() { return &new_born_cell_grid_; }


	// selected cell management
	bool deselect_cell();
	const Cell* get_selected_cell() const { return all_cells_.at(selected_cell_id_); }

	// public statistics
	CellManagerStatistics& get_statistics() { return statistics_; }
	const std::vector<float>& get_generation_distribution();
	void update_100frame_stats(int iterations);
	void update_statistics();


private: // only functions this class can access
	// Utility

	template<typename T>
	void gather_objects_in_radius(FixedSpan<cell_idx, uint16_t>& indexes, const o_vector<T>& objects, const sf::Vector2f& position, const float radius);

	void check_for_extinction_event();
	void inject_selected_protozoa(bool is_energy, float amount);

	// statistics 
	void register_death_stat(const float lifetime, const bool had_offspring);
	void register_birth_stat();
	float calculate_average_generation() const;

	// updating
	void update_springs(bool immune);
	void update_cells();
	void update_cell_matter();
	void update_cell(Cell* cell);
	void impulse_tax_cell(Cell* cell, const float impulse) const;
	void collect_connection_requests();

	void ensure_update_jobs_built();

	// newly born cells
	void add_new_cells_to_grid();
	void try_connect_newborn_cell(Cell* cell);


	// birth - springs
	int32_t create_spring(const uint32_t cell_a_id, const uint32_t cell_b_id);
	void create_weak_offspring(uint32_t parent_id);
	void create_temporary_spring_connection(const cell_idx parent_id, const cell_idx offspring_id);
	void apply_matter_birth_requests();
	void apply_connection_requests();

	// birth - cells
	CellBodyPair create_cell(sf::Vector2f position = { 0, 0 }, bool random_genetics = false);
	void collect_reproduction_requests();
	void apply_reproduction_requests();

	// birth - whole protozoa
	void clone_selected_protozoa();
	bool can_protozoa_reproduce(const cell_idx start_id, FixedSpan<cell_idx>& out_cell_ids);
	void create_protozoa_from_pool(const sf::Vector2f position, const unsigned max_cells, const unsigned max_springs);
	bool protozoa_reproduce(FixedSpan<cell_idx>& old_ids);

	sf::Vector2f calculate_organism_centre(FixedSpan<cell_idx>& cell_ids);
	float        calculate_organism_radius(FixedSpan<cell_idx>& cell_ids, const sf::Vector2f& centre);
	sf::Vector2f calculate_clone_offset(FixedSpan<cell_idx>& parent_ids);

	int32_t clone_cell(cell_idx parent_cell_id, const sf::Vector2f& offset);
	bool    clone_spring(uint32_t parent_spring_id, cell_idx offspring_cell_a, cell_idx offspring_cell_b);

	// death - cell matter
	void collect_matter_death_requests();
	void apply_matter_death_requests();
	void remove_cell_matter(cell_idx matter_id);


	// death - cell
	void collect_cell_death_requests();
	void apply_cell_death_requests();
	void remove_cell(cell_idx cell_id);
	void speed_tax_cell(Cell* cell);

	// death - springs
	void collect_spring_death_requests();
	void apply_spring_death_requests();

	void check_for_dangling_springs();
	void remove_spring(uint32_t spring_id);
};