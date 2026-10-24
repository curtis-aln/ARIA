#pragma once
#include <cstdint>

struct CellMatterSettings
{
	inline static constexpr float transparency_outer = 175.f;
	inline static constexpr float transparency_inner = 150.f;

	inline static constexpr float    friction = 0.8f;
	inline static uint16_t max_time_to_live = 13'000; // frames

	inline static constexpr uint8_t max_radius = 180;
	inline static constexpr uint8_t min_radius = 120;

	inline static constexpr uint8_t max_gray = 70;
	inline static constexpr uint8_t min_gray = 40;

	inline static constexpr float spawn_chance = 0.75f;

	inline static constexpr float color_transition_frames = 300.f;
	inline static constexpr float inv_color_transition_frames = 1.f / color_transition_frames; // avoid per-frame division
};