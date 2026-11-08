#pragma once

#include "spring.h"

struct Cell;

// Organics: everything the spring does to the cells it connects, including random death,
// stress damage, nutrient transfer, clock syncing and the energy cost of working.
// Reads the physics results (stress, work_done) that Spring::update_physics() produced.
//
// This is the type that goes in your containers, since a bare Spring has no organics.
// Never store these by value in a vector<Tendon> or o_vector<Tendon>: that slices off death_chance_.
struct Tendon : Spring
{
	using Spring::Spring; // inherit the constructor

	bool broken = false;
	float extension_ratio = 0.f;

	// The spring's genetic information, determined by its two connecting cells
	TendonGenome genome{};

	float death_chance_ = 0.f; // zero means no chance of breaking, 1 means guaranteed breakage on the first frame

	void reset(); // hides Spring::reset(): resets the base, then the organics state

	void break_tendon() { broken = true; }
	bool is_tendon_broken() const { return broken; }

	[[nodiscard]] float get_spring_constant() const;

	void death_update(Cell& cell_a, Cell& cell_b);
	void update_organics(Cell& cell_a, Cell& cell_b, bool stress_damage, bool work_done_energy);
	void update_integrity(Cell& cell_a, Cell& cell_b);

	void create_offspring(Tendon& offspring) { offspring.genome.copy_genetics(genome); }

private:
	// takes in the nutrients of cell a and cell b and transfers between them,
	// respecting max_nutrients caps and applying a fixed % loss per transfer
	struct NutrientFlow { float* sender = nullptr; float* receiver = nullptr; bool priority = false; };

	NutrientFlow pick_nutrient_flow(Cell& cell_a, Cell& cell_b) const;
	float calc_nutrient_send_amount(const NutrientFlow& flow, float rate) const;
	void transfer_nutrients(Cell& cell_a, Cell& cell_b);
	void sync_clocks(Cell& cell_a, Cell& cell_b);
	float target_extension_fraction(const int internal_clock);
};