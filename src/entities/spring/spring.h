#pragma once

#include <cstdint>
#include <SFML/System/Vector2.hpp>

#include <entities/cell/cell_genome.h>
#include "spring_settings.h"

struct Cell;

struct SpringResult { float work_done; float force_magnitude; bool broken; };

struct Spring : SpringSettings
{
private:
	bool broken = false;

public:
	// unique spring ID, used for genome referencing, must not change during the spring's lifetime
	uint32_t id_{};

	// The springs genetic information, determined by its two connecting cells
	SpringGenome genome{};

	// we store the id's of the cells here so whe we call update in the main class we know where to look for the cells, relative to the protozoa
	uint32_t cell_A_id{};
	uint32_t cell_B_id{};

	uint32_t internal_clock_{};
	float death_chance_ = 0.f; // zero means no chance of breaking, 1 means guaranteed breakage on the first frame

	float work_done = 0.f;
	float rest_length = 0.f;
	float current_length = 0.f;
	float ratio = 0.f;

	float spring_force = {};
	float damping_force = {};

	float stress = 0.f; // 0..1, normalised force relative to break threshold

	sf::Vector2f movement_vector{ 0 ,0 };

	// init list is in member declaration order (id_ is declared before the cell ids)
	Spring(const uint8_t _id = 0, const uint8_t _cell_A_id = 0, const uint8_t _cell_B_id = 0)
		: id_(_id), cell_A_id(_cell_A_id), cell_B_id(_cell_B_id)
	{}

	void reset();

	void break_spring() { broken = true; }
	bool is_spring_broken() const { return broken; }

	void create_offspring(Spring& offspring) { offspring.genome.copy_genetics(genome); }

	// updates movement_vector, stress, work_done etc. from the positions/velocities of the two cells
	void update_physics(
		const sf::Vector2f& pos_a, const sf::Vector2f& vel_a,
		const sf::Vector2f& pos_b, const sf::Vector2f& vel_b,
		bool disable_length_breakage, bool disable_force_breakage, float min_rest_length, const float reduced_mass);

	void death_update(Cell& cell_a, Cell& cell_b);
	void update_organics(Cell& cell_a, Cell& cell_b, bool stress_damage, bool work_done_energy);
	void update_integrity(Cell& cell_a, Cell& cell_b);

private:
	[[nodiscard]] float get_spring_constant();

	// takes in the nutrients of cell a and cell b and transfers between them,
	// respecting max_nutrients caps and applying a fixed % loss per transfer
	struct NutrientFlow { float* sender = nullptr; float* receiver = nullptr; bool priority = false; };

	NutrientFlow pick_nutrient_flow(Cell& cell_a, Cell& cell_b) const;
	float calc_nutrient_send_amount(const NutrientFlow& flow, float rate) const;
	void transfer_nutrients(Cell& cell_a, Cell& cell_b);
	void sync_clocks(Cell& cell_a, Cell& cell_b);

	float calculate_rest_length(int internal_clock);
};