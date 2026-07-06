#pragma once

struct SimulationState
{
	float max_frame_rate_updating = 0.f; // 0 = unlimited
	float max_frame_rate_rendering = 0.f;
	float total_time_elapsed = 0.f;

	float camera_zoom = 1.f;

	float mouse_pos_x = 0.f;
	float mouse_pos_y = 0.f;

	// Statistics
	float rendering_frame_rate = 0.f;
	float updating_frame_rate = 0.f;
};