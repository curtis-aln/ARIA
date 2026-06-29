#pragma once

#include "../../world/world_state.h"
#include "simulation/imgui/population_history.h"
#include "../../managers/cell_manager/organism_tracker.h"
#include "managers/food_manager/food_data.h"


struct SimSnapshot
{
    WorldToggles toggles;
    WorldStatistics stats;
    RenderData render;
    FoodData food_data;

    SpatialGridData food_grid;
    SpatialGridData cell_grid;
    PopulationHistory history;

    bool selected_a_cell = false;
    OrganismTracker protozoa_tracker{};

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
