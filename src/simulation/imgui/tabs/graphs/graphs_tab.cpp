#include "graphs_tab.h"
#include "population_history.h"
#include <algorithm>
#include <imgui.h>

// ─────────────────────────────────────────────────────────────────────────────
//  BandCache
// ─────────────────────────────────────────────────────────────────────────────
void GraphsTab::BandCache::refresh(const PopulationHistory& h, const bool need_protozoa, const bool need_food)
{
    if (h.size() == valid_for_n) return;
    valid_for_n = h.size();

    if (need_protozoa) PopulationHistory::compute_band(h.protozoa, plo, phi);
    if (need_food)     PopulationHistory::compute_band(h.food, flo, fhi);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Top-level
// ─────────────────────────────────────────────────────────────────────────────
void GraphsTab::draw(const SimSnapshot& snap, ImGuiContext& ctx)
{
    const float total_w = ImGui::GetContentRegionAvail().x;
    const float left_w = std::clamp(total_w * k_left_panel_frac, k_left_panel_min, k_left_panel_max);
    const float ch = -ImGui::GetFrameHeight();

    ImGui::BeginChild("ST_left", { left_w, ch }, false);
    draw_stat_panels(snap);
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("ST_right", { -1.f, ch }, false);

    draw_shared_toolbar(snap);

    // BUGFIX: the original returned early here without calling EndChild(),
    // which trips ImGui's begin/end stack assert whenever the tab bar is not visible.
    if (ImGui::BeginTabBar("##graph_tabs"))
    {
        if (ImGui::BeginTabItem("Population")) { draw_population_tab(snap);  ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Generations")) { draw_generations_tab(snap); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Misc")) { draw_misc_tab(snap);        ImGui::EndTabItem(); }

        ImGui::EndTabBar();
    }

    ImGui::EndChild();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Shared toolbar
// ─────────────────────────────────────────────────────────────────────────────
void GraphsTab::draw_shared_toolbar(const SimSnapshot& snap)
{
    const PopulationHistory& history = snap.history;
    const float live_x = static_cast<float>(history.current_frame());

    ImGui::SetNextItemWidth(400.f);
    ImGui::SliderFloat("Window (iterations)##g", &m_scroll_window_,
        k_scroll_window_min, k_scroll_window_max, "%.0f");
    ImGui::SameLine(0, 16);

    if (m_recording_)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, { 0.7f, 0.2f, 0.2f, 1.f });
        if (ImGui::Button("Stop recording"))
        {
            m_recording_ = false;
            //history.add_manual_event(live_x, "record end"); todo
        }
        ImGui::PopStyleColor();
    }
    else if (ImGui::Button("Record from here"))
    {
        m_recording_ = true;
        m_record_start_ = live_x;
        //history.add_manual_event(live_x, "record start", { 0.4f, 0.8f, 1.f, 1.f }); todo
    }

    ImGui::SameLine(0, 16);
    if (ImGui::Button("Export CSV"))
        history.export_csv("population_export.csv");
}