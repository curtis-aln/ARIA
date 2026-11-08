#pragma once

#include <cstdint>
#include <SFML/System/Vector2.hpp>

#include <entities/cell/cell_genome.h>
#include "spring_settings.h"
#include "../../Utils/fast_funcs.h"

struct CellPosVel
{
	sf::Vector2f cell_a_pos{};
	sf::Vector2f cell_a_vel{};

	sf::Vector2f cell_b_pos{};
	sf::Vector2f cell_b_vel{};
};

struct physics_toggles
{
	bool disable_length_breakage = false;
	bool disable_force_breakage = false;
};

struct physics_stats
{
	const float extension_fraction = 0.f; 
	const float spring_constant = 0.f;
	const float damping = 0.f;
	const float min_rest_length = 0.f;
	const float reduced_mass = 0.f;
};


struct SpringResult { float work_done; float force_magnitude; bool broken; };

// Physics only: rest length oscillation, spring + damping force, stress, work done and breakage.
// Knows nothing about cells beyond the positions and velocities handed to update_physics().
// Everything a spring does TO cells (nutrients, clocks, damage, energy) lives in Tendon.
//
// No virtual functions: keep it that way so Spring and Tendon stay trivially copyable.
struct Spring : SpringSettings
{	
public:
	// unique spring ID, used for genome referencing, must not change during the spring's lifetime
	uint32_t id_{};

	// ids of the connected cells, so the caller knows where to look them up
	uint32_t cell_A_id{};
	uint32_t cell_B_id{};

	float work_done = 0.f;
	float rest_length = 0.f;
	float current_length = 0.f;

	float spring_force = {};
	float damping_force = {};

	float stress = 0.f; // 0..1, normalised force relative to break threshold
	float strain = 0.f; // 0 = cells touching, 1 = at length breaking point

	sf::Vector2f movement_vector{ 0, 0 };

	// init list is in member declaration order (id_ is declared before the cell ids)
	Spring(const uint32_t _id = 0, const uint32_t _cell_A_id = 0, const uint32_t _cell_B_id = 0)
		: id_(_id), cell_A_id(_cell_A_id), cell_B_id(_cell_B_id)
	{}

	void reset();

	// updates movement_vector, stress, work_done etc. from the positions/velocities of the two cells
	void update_physics(const CellPosVel& cell_data, const physics_toggles& toggles, const physics_stats& stats);
};