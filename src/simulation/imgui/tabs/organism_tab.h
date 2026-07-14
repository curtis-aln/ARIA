#pragma once
#include "i_tab.h"

// One genetic sine parameter: its current value, valid range, display format,
// and the SimCommand type that updates it.
struct WaveParam
{
    float       value;
    float       min;
    float       max;
    const char* fmt;
    CommandType type;
};

class OrganismTab : public ITab
{
public:
    const char* label() const override { return "Organism"; }
    void        draw(const SimSnapshot& snap, ImGuiContext& ctx)   override;

private:
    int  m_last_id_ = -1;
    int  m_sel_cell_idx_ = 0;
    int  m_sel_spring_idx_ = 0;
    bool m_sel_is_spring_ = false;
    int  m_wave_cycles_ = 8;   // number of full periods shown in the sinwave graph

    // ── Feed state (owned here, shared by Energy tab) ─────────────────────
    int   m_feed_mode_ = 0;     // 0 = energy,  1 = nutrients
    int   cell_selection_mode = 0;
    float m_feed_amount_ = 50.f;

    void draw_overview(const SimSnapshot& snap, ImGuiContext& ctx, const OrganismTracker& protozoa);
    static void draw_no_selection();
    void draw_cells_springs_tab(const SimSnapshot& snap, ImGuiContext& ctx, const OrganismTracker& protozoa);
    void draw_wave_panel(ImGuiContext& ctx, const char* child_id, const char* description, int frames_alive, int idx, const char* value_label, std::vector<float>& scratch_buf, const WaveParam& amplitude, const WaveParam& frequency, const WaveParam& offset, const WaveParam& vertical_shift);
    void draw_cell_detail(ImGuiContext& ctx, const Cell& c, const sf::Vector2f& pos, const sf::Vector2f& vel);
    void draw_spring_detail(ImGuiContext& ctx, const OrganismTracker& p, const Spring& s);
    void draw_energy_tab(ImGuiContext& ctx, const SimSnapshot& snap);
};