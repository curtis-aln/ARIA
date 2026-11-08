#pragma once

#include "state.h"
#include "simulation/imgui/tabs/graphs/population_history.h"
#include "../../managers/cell_manager/organism_tracker/organism_tracker.h"


struct SimSnapshot
{
	SimulationToggles sim_toggles;
	CellManagerToggles cell_toggles;  
    WorldToggles world_toggles;
    FoodToggles  food_toggles;

    // Statistics
    SimulationStatistics sim_stats;
    WorldStatistics world_stats;
	CellManagerStatistics cell_manager_stats;
	FoodManagerStatistics food_manager_stats;

    RenderData render;

    SpatialGridData food_grid;
    SpatialGridData cell_grid;
    PopulationHistory history;

    OrganismTracker protozoa_tracker{};

	SimSnapshot() = default;

    SimSnapshot(int cell_render_reserve)
    {
	    render.inner_colors.reserve(cell_render_reserve);
	    render.outer_colors.reserve(cell_render_reserve);
	    render.positions.reserve(cell_render_reserve);
	    render.radii.reserve(cell_render_reserve);
    }
};
