#pragma once

struct SpringSettings
{
	inline static float maximum_extension;
	inline static float minimum_extension;

	inline static float spring_work_const; // how we scale the energy cost of springs
	inline static float spring_break_force;
	inline static float spring_break_length_factor;
	inline static float spring_damage_threshold;
	
	inline static float fully_developed_age; // arbitrary age at which the spring is considered fully developed
	inline static float nutrients_transfer_loss;

	inline static float stress_damage_const;

	inline static constexpr float spring_work_energy_loss_k = 0.1f;

};