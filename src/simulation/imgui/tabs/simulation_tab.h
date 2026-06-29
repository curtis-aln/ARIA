#pragma once
#include "i_tab.h"

class SimulationTab : public ITab
{
public:
    const char* label() const override { return "Simulation"; }
    void        draw(const SimSnapshot& snap, ImGuiContext& ctx)   override;

private:
    enum class FFCondition { Duration, Population, Generation };
    FFCondition m_ff_cond_ = FFCondition::Duration;
    float       m_ff_target_ = 300.f;
    bool        m_fast_fwd_ = false;
    float       m_speed_ = 1.f;


    int   m_mouse_mode_ = 0;      // 0 = Add, 1 = Remove
    bool  m_add_cells_       = true;
    bool  m_add_food_        = false;
    bool  m_remove_cells_    = true;
    bool  m_remove_food_     = false;
    float m_mouse_intensity_ = 1.f;
    float m_mouse_radius_    = 100.f;
};