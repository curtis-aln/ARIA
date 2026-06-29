#pragma once

struct CellManagerSettings
{
	inline static unsigned max_protozoa;
	inline static unsigned initial_protozoa;
	inline static bool auto_reset_on_extinction;

	inline static constexpr int max_evolutionary_iterations = 5;
	inline static constexpr int desired_cell_count = 3;
};