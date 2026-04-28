#pragma once
#include "i_tab.h"
#include "../population_history.h"
#include "context/sim_snapshot.h"

class DisplayTab : public ITab
{
public:
    const char* label() const override { return "Display"; }
    void        draw(const SimSnapshot& snap, ImGuiContext& ctx) override;

private:
    int   m_camera_lock_ = 0;
    float m_zoom_slider_ = 1.f;
    float m_ui_scale_ = 100.f;
};
