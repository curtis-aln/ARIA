#pragma once
#include "i_tab.h"
#include "context/sim_snapshot.h"

class StatisticsTab : public ITab
{
public:
    const char* label() const override { return "Statistics"; }
    void        draw(const SimSnapshot& snap, ImGuiContext& ctx)   override;
};
