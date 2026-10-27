#pragma once
#include <cstdint>

struct CellSettings
{
	inline static float spawn_radius; // during the protozoa creation, cells are spawned in a circle of this radius
	inline static float bite_amount; // how much nutrients a cell can take from food in one bite

	inline static size_t repro_cooldown; // how long a cell has to wait before it can reproduce again
	inline static float digestive_time; // how long it takes for a cell to digest food before it can eat again

	inline static int infant_time; // how long a cell is an infant for
	inline static constexpr int infant_check_interval = 20; // how often to check for newborn connections, in frames

	// Energy, nutrients, and integrity
	inline static float initial_energy; // energy the protozoa spawn with
	inline static float initial_integrity = 100.f;

	inline static float max_nutrients = 100;
	inline static float max_integrity = 100;
	inline static float max_energy = 100;

	inline static float integrity_conversion_rate;
	inline static float nutrients_conversion_rate;

	inline static float friction_energy_loss_const;

	inline static int death_age_threshold = 8000;
	inline static float death_integrity_rate = 0.004f;
	inline static float death_decay_growth = 0.001f; // extra multiplier per tick past the threshold


	inline static float drag_min = 0.00f; // glide: ~0.5% speed lost per tick
	inline static float drag_max = 0.15f;  // grip: ~5% speed lost per tick

};