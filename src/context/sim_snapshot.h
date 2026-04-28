#pragma once

#include "../world/world_state.h"
#include "../Protozoa/Protozoa.h"
#include "imgui/population_history.h"

// ─────────────────────────────────────────────────────────────────────────────
// Immutable world state packaged by the sim thread after each tick.
// Read by the render thread via TripleBuffer — never written to by the
// render thread. Contains everything the UI and renderer need for one frame:
// toggles, stats, render geometry, food, grid debug data, population history,
// and a copy of the currently selected Protozoa (if any).
// ─────────────────────────────────────────────────────────────────────────────
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
    Protozoa protozoa{-1};
};