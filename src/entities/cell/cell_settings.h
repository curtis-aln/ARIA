#pragma once

struct CellSettings
{
	inline static float integrity = 100.f;
	inline static float integrity_conversion_rate = 0.05f;
	inline static float offspring_energy_cost = 100.f;
	inline static float reproduce_energy_thresh = 200.f;
	inline static float conversion_rate = 0.45f;

	inline static int max_cells;

	inline static float spawn_radius;

	inline static float energy_decay_rate; // how quickly energy decays per frame

	inline static float wander_threshold; // if a cell wanders too far away from the protozoa it kills the whole thing

	inline static size_t reproductive_cooldown;
	inline static float digestive_time; // per cell

	inline static float initial_energy; // energy the protozoa spawn with

};