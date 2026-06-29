#pragma once

struct SpringSettings
{
	inline static float energy_share_rate = 0.1f;

	inline static float spring_break_force = 580.f;
	inline static float spring_damage_threshold = 20.f;

	inline static float breaking_length;
	inline static float maximum_extension;

	inline static float spring_work_const; // how we scale the energy cost of springs
};