#pragma once

struct CellManagerSettings
{
	inline static unsigned max_cells;
	inline static unsigned initial_protozoa; // seeds spawned at the start of the simulation
	inline static bool auto_reset_on_extinction;

	inline static int infant_time; // how long a cell is an infant for
	inline static float speed_energy_tax; // The Tax for not traveling fast enough

	// DAMAGE
	inline static float impulse_damage_thresh = 20.0f;
	inline static float impulse_damage_multiplier = 0.00025f;
	inline static float max_single_hit_integrity_fraction = 0.2f;

	// STATISTICS
	inline static constexpr size_t max_lifetime_samples_ = 500;
	inline static constexpr int survival_rate_window_size_ = 100;

	// MISC
	inline static constexpr int infant_check_interval = 20; // how often to check for newborn connections, in frames
	inline static constexpr float delta_min_speed = 0.f; //1.f / 3'000.f;
	inline static constexpr int extincion_threshold = 0; // if there are less than this number of protozoas, we consider it an extinction event
	inline static constexpr int init_spring_immunity_time = 400; // for the start of the sim the springs are immune to damage


	// GRAPHICS
	inline static constexpr float cell_outline_thickness = 1.3f;
};