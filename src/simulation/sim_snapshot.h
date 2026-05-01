#pragma once

#include "../world/world_state.h"
#include "imgui/population_history.h"
#include "../world/protozoa_tracker.h"
#include "Food/food_data.h"


struct SimSnapshot
{
    WorldToggles toggles;
    WorldStatistics stats;
    RenderData render;
    FoodData food_data;

    SpatialGridData food_grid;
    SpatialGridData cell_grid;
    PopulationHistory history;

    bool selected_a_protozoa = false;
    ProtozoaTracker protozoa_tracker{};

	SimSnapshot() = default;

    SimSnapshot(int cell_render_reserve)
    {
	    render.inner_colors.reserve(cell_render_reserve);
	    render.outer_colors.reserve(cell_render_reserve);
	    render.positions_x.reserve(cell_render_reserve);
	    render.positions_y.reserve(cell_render_reserve);
	    render.radii.reserve(cell_render_reserve);
    }
};
