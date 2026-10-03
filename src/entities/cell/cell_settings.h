#pragma once
#include <cstdint>

struct CellSettings
{
	inline static float spawn_radius; // during the protozoa creation, cells are spawned in a circle of this radius
	inline static float bite_amount; // how much nutrients a cell can take from food in one bite

	inline static size_t repro_cooldown; // how long a cell has to wait before it can reproduce again
	inline static float digestive_time; // how long it takes for a cell to digest food before it can eat again

	// Energy, nutrients, and integrity
	inline static float initial_energy; // energy the protozoa spawn with
	inline static float initial_integrity = 100.f;

	inline static float max_nutrients = 100;
	inline static float max_integrity = 100;
	inline static float max_energy = 100;

	inline static float integrity_conversion_rate;
	inline static float nutrients_conversion_rate;

	inline static float friction_energy_loss_const;
	inline static constexpr uint8_t max_cell_connections = 2;

	inline static int death_age_threshold = 2700;
	inline static float death_energy_rate = 0.001f;
};