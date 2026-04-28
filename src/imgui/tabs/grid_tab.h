#pragma once
#include "i_tab.h"
#include "../../context/sim_snapshot.h"

class GridTab : public ITab
{
public:
    const char* label() const override { return "Grid"; }
    void        draw(const SimSnapshot& snap, ImGuiContext& ctx)   override;

private:
    //void draw_tuning(SimSnapshot& snapshot);
    void draw_grid_info(const char* label, const SpatialGridData& grid, bool tracking);
};