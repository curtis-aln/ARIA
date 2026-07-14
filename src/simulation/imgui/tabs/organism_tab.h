#pragma once
#include "i_tab.h"

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
    float m_feed_amount_ = 50.f;

    void draw_overview(const SimSnapshot& snap, ImGuiContext& ctx, const OrganismTracker& protozoa);
    static void draw_no_selection();
    void draw_cells_springs_tab(const SimSnapshot& snap, ImGuiContext& ctx, const OrganismTracker& protozoa);
    void draw_cell_detail(ImGuiContext& ctx, const Cell& c, const sf::Vector2f& pos, const sf::Vector2f& vel);
    void draw_spring_detail(ImGuiContext& ctx, const OrganismTracker& p, const Spring& s);
    void draw_energy_tab(ImGuiContext& ctx, const SimSnapshot& snap);
};