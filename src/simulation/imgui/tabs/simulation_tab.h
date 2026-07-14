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
    bool  m_cells_       = true;
    bool  m_food_        = false;
    bool  m_remove_cells_    = true;
    bool  m_remove_food_     = false;
	bool  m_show_influence_radius_ = false;

    int m_mouse_intensity_ = 1;
    float m_mouse_radius_    = 100.f;

    float m_spring_breaking_force_ = 0.f;
    float m_spring_breaking_length_ = 0.f;
    float m_spring_damage_threshold_ = 0.f;
    float m_spring_work_const_ = 0.f;

    int   m_camera_lock_ = 0;
    float m_zoom_slider_ = 1.f;
    float m_camera_friction_ = 0.98f;
    float m_ui_scale_ = 100.f;

    bool  m_blackhole_ = false;
    float m_bh_strength_ = 500.f;
    float m_bh_radius_ = 1000.f;
};