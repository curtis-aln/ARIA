#pragma once
#include "i_tab.h"
#include "../../../Utils/o_vec/o_vec_debug.h"

// ─────────────────────────────────────────────────────────────────────────────
//  OVecDebugTab  —  ImGui control-panel tab that visualises all four
//  OVecDebug instances live:  cells, food, bodies (cells+food), and springs.
//
//  Sub-tabs
//  ┌────────────┬───────────────────────────────────────────────────────────┐
//  │ Overview   │ Side-by-side capacity + fill bars for all four vectors    │
//  │ Capacity   │ Detailed slot counts, memory usage, resize events         │
//  │ Churn      │ Per-slot reuse heatmap + top-N most recycled slots        │
//  │ Snapshots  │ Take / diff named snapshots; delta table                  │
//  │ Integrity  │ Run integrity checks on-demand, show pass/fail log        │
//  └────────────┴───────────────────────────────────────────────────────────┘
//o_vec
//  Usage:
//      OVecDebugTab tab(dbg_cells, dbg_food, dbg_bodies, dbg_springs);
//      // In your draw loop:
//      tab.draw(snap, ctx);
// ─────────────────────────────────────────────────────────────────────────────

// Forward-declare the four object types so we can hold typed debug refs.
struct Cell;
struct Food;
struct Body;
struct Spring;

class OVecDebugTab : public ITab
{
public:
    // ── Construction ─────────────────────────────────────────────────────────

    OVecDebugTab();

    // ── ITab interface ────────────────────────────────────────────────────────

    const char* label() const override { return "O_Vec Debug"; }
    void        draw(const SimSnapshot& snap, ImGuiContext& ctx) override;

private:
    // ── Typed debug references (non-owning) ──────────────────────────────────

    OVecDebug<Cell> m_dbg_cells_;
    OVecDebug<Food> m_dbg_food_;
    OVecDebug<Body> m_dbg_bodies_;
    OVecDebug<Spring> m_dbg_springs_;

    // ── Per-tab state ─────────────────────────────────────────────────────────

    // Snapshot sub-tab: indices for the two snapshots being diffed
    int  m_snap_a_idx_ = 0;
    int  m_snap_b_idx_ = 0;

    // Snapshot sub-tab: which vector's snapshot list is shown (0-3)
    int  m_snap_vec_sel_ = 0;

    // Churn sub-tab: top-N slider
    int  m_churn_top_n_ = 20;

    // Churn sub-tab: which vector to inspect (0-3)
    int  m_churn_vec_sel_ = 0;

    // Integrity sub-tab: accumulated log lines
    std::vector<std::string> m_integrity_log_;
    bool m_integrity_all_pass_ = true;

    // ── Sub-tab draw functions ────────────────────────────────────────────────

    void draw_overview_tab(const SimSnapshot& snap, ImGuiContext& ctx);
    void draw_capacity_tab(const SimSnapshot& snap, ImGuiContext& ctx);
    void draw_churn_tab(const SimSnapshot& snap, ImGuiContext& ctx);
    void draw_snapshots_tab(const SimSnapshot& snap, ImGuiContext& ctx);
    void draw_integrity_tab(const SimSnapshot& snap, ImGuiContext& ctx);
};