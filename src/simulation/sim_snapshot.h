#pragma once

#include "../world/world_state.h"
#include "../Protozoa/Protozoa.h"
#include "imgui/population_history.h"


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