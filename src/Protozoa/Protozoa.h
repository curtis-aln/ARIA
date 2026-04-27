#pragma once
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include "../settings.h"
#include "../Utils/Graphics/Circle.h"
#include "cell.h"
#include "spring.h"


#include "../Food/food_manager.h"
#include "genetics/GenomeManager.h"

// this is the organisms in the simulation, they are made up of cells which all act independently, attached by springs
// the protozoa class is responsible for connecting the springs and cells



// The Genome class handles anything that has to do with mutation and genetics
class Protozoa : ProtozoaSettings, public GenomeManager
{
public:
	int id = 0;
	bool active = true; // for o_vector.h

	// statistics informaiton
	uint8_t time_since_last_reproduced = 0;
	sf::Vector2f birth_location = { 0, 0 };

	float energy_lost_to_springs = 0.f;

	uint8_t offspring_count = 0;
	uint16_t frames_alive = 0u;
	uint8_t total_food_eaten = 0;

private:
	sf::RenderWindow* m_window_ = nullptr;
	Circle* m_world_bounds_ = nullptr;

	// components of the protozoa
	std::vector<Cell> m_cells_{};
	std::vector<Spring> m_springs_{};	


	uint8_t stomach = 0;
	float energy = initial_energy;
	
	// calculated params
	sf::Rect<float> m_personal_bounds_{};

	bool reproduce = false;
	bool dead = false;
	

	
public:
	bool immortal = false;

	Protozoa(int id_ = 0, Circle* world_bounds = nullptr, sf::RenderWindow* window = nullptr);

	Protozoa(const Protozoa& other);
	Protozoa& operator=(const Protozoa& other);

	static void copy_protozoa_data(Protozoa& dst, const Protozoa& src);

	void update(FoodManager& food_manager, bool debug, float min_speed);
	
	void mutate(const bool artificial_add_cell = false, const float artificial_mutatation_rate = 0.f, const float artificial_mutatation_range = 0.f);
	
	void render_debug(Font* font, bool skeleton_mode, bool show_connections, bool show_bounding_boxes);

	// debugging and modifying settings
	int check_mouse_press(sf::Vector2f mousePosition, bool tollarance_check = false) const;
	void move_selected_cell(sf::Vector2f mouse_position);

	// information fetching
	Cell* get_selected_cell(sf::Vector2f mouse_pos);

	std::vector<Cell>& get_cells() { return m_cells_; }
	const std::vector<Cell>& get_cells() const { return m_cells_; }
	std::vector<Spring>& get_springs() { return m_springs_; }
	const std::vector<Spring>& get_springs() const { return m_springs_; }

	sf::Rect<float> get_bounds() const { return m_personal_bounds_; }
	sf::Vector2f get_center() const;
	bool is_alive() const { return !dead; }
	bool should_reproduce() const { return reproduce; }
	float get_energy() const { return energy; }
	unsigned stomach_capacity() const { return stomach; }
	size_t stomach_reproduce_thresh() const { return m_cells_.size(); }
	size_t reproductive_cooldown_calculator() const { return reproductive_cooldown / m_cells_.size(); }
	int get_cell_count() const { return (int)m_cells_.size(); }
	int get_spring_count() const { return (int)m_springs_.size(); }

	
	void set_render_window(sf::RenderWindow* window){m_window_ = window;}
	void set_bounds(Circle* bounds){m_world_bounds_ = bounds;}

	// information setting
	void init_one_cell();
	void move_center_location_to(const sf::Vector2f new_center);
	void set_protozoa_attributes(Protozoa* other);	
	void resolve_collisions(const std::vector<sf::Vector2f>& collision_resolutions, int& idx);
	void create_offspring(Protozoa* parent, bool should_mutate = true);
	void force_reproduce();
	void inject(const float energy_injected);

	void kill();
	void bound_cells();

	void soft_reset();
	void hard_reset();

	void add_spring();
	void add_cell();

	void remove_cell();
	void remove_spring();


private:
	// rendering
	void draw_cell_physics(Font* font);
	void draw_spring_information(Font* font) const;
	void render_cell_connections(Cell& cell, bool thick_lines = false) const;
	void render_protozoa_springs();

	// debugging
	void nearby_food_information() const;
	void draw_cell_outlines();

	// updating
	void update_bounding_box();
	void update_springs();
	void update_cells();
	void update_generation();

	// organic
	void reproduce_check();
	void handle_food(FoodManager& food_manager, bool debug);
	void record_nearby_food(Food* food);
	void consume(Food* food, FoodManager& food_manager);
	

	// mutating
	void mutate_existing_cells(float mut_rate = 0.f, float mut_range = 0.f);
	void mutate_existing_springs(float mut_rate = 0.f, float mut_range = 0.f);
	bool cell_wander_check(Cell& cell);

	void check_death_conditions(float min_speed);

};
