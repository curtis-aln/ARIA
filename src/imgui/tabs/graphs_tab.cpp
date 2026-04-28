#include "graphs_tab.h"
#include "../population_history.h"
#include <imgui.h>
#include <implot.h>
#include <numeric>
#include <algorithm>
#include <cfloat>

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
//  Visible-range helper
// ─────────────────────────────────────────────────────────────────────────────
bool GraphsTab::visible_range(const std::vector<float>& times,
    const std::vector<float>& data,
    const float x_min, const float x_max,
    float& out_lo, float& out_hi)
{
    out_lo = FLT_MAX;
    out_hi = -FLT_MAX;
    const int n = static_cast<int>(std::min(times.size(), data.size()));
    for (int i = 0; i < n; ++i)
    {
        if (times[i] < x_min || times[i] > x_max) continue;
        out_lo = std::min(out_lo, data[i]);
        out_hi = std::max(out_hi, data[i]);
    }
    if (out_lo == FLT_MAX) return false;

    // Pad so the line doesn't kiss the axis edges.
    const float pad = std::max((out_hi - out_lo) * 0.12f, 0.01f);
    out_lo -= pad;
    out_hi += pad;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Top-level
// ─────────────────────────────────────────────────────────────────────────────
void GraphsTab::draw(const SimSnapshot& snap, ImGuiContext& ctx)
{
    draw_shared_toolbar(snap);
    if (!ImGui::BeginTabBar("##graph_tabs")) return;

    if (ImGui::BeginTabItem("Population")) { draw_population_tab(snap);  ImGui::EndTabItem(); }
    if (ImGui::BeginTabItem("Generations")) { draw_generations_tab(snap); ImGui::EndTabItem(); }
    if (ImGui::BeginTabItem("Misc")) { draw_misc_tab(snap);        ImGui::EndTabItem(); }

    ImGui::EndTabBar();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Shared toolbar
// ─────────────────────────────────────────────────────────────────────────────
void GraphsTab::draw_shared_toolbar(const SimSnapshot& snap)
{
	const PopulationHistory& history = snap.history;
    const float live_x = snap.stats.iterations_;

    ImGui::SetNextItemWidth(300.f);
    ImGui::SliderFloat("Window (s)##g", &m_scroll_window_, 150.f, 4500.f, "%.0fs");
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

// ─────────────────────────────────────────────────────────────────────────────
//  Population tab
// ─────────────────────────────────────────────────────────────────────────────
void GraphsTab::draw_population_tab(const SimSnapshot& snap)
{
	const PopulationHistory& history = snap.history;
    ImGui::Checkbox("Protozoa", &m_show_protozoa_); ImGui::SameLine(0, 12);
    ImGui::Checkbox("Food", &m_show_food_);     ImGui::SameLine(0, 12);
    ImGui::Checkbox("Total", &m_show_total_);    ImGui::SameLine(0, 12);
    ImGui::Checkbox("Bands", &m_show_bands_);

    const float live_x = snap.stats.iterations_;
    const float x_max = m_hover_paused_ ? m_paused_x_max_ : live_x;
    const float x_min = x_max - m_scroll_window_;

    // Only recompute bands when sample count actually changes.
    if (m_show_bands_)
        m_band_cache_.refresh(history, m_show_protozoa_, m_show_food_);

    constexpr ImPlotFlags     pf = ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMouseText;
    constexpr ImPlotAxisFlags yf = ImPlotAxisFlags_AutoFit;

    if (!ImPlot::BeginPlot("##pop", { -1.f, -1.f }, pf)) return;

    ImPlot::SetupAxes("Time (s)", "Count", ImPlotAxisFlags_None, yf);
    ImPlot::SetupAxisLimits(ImAxis_X1, x_min, x_max, ImGuiCond_Always);

    const int n = static_cast<int>(history.size());
    if (n > 0)
    {
        const float* t = history.time.data();

        if (m_show_bands_ && m_band_cache_.valid_for_n == history.size())
        {
            if (m_show_protozoa_ && !m_band_cache_.plo.empty())
            {
                ImPlot::SetNextFillStyle({ 0.3f, 0.6f, 1.f, 1.f }, 0.15f);
                ImPlot::PlotShaded("##pb", t, m_band_cache_.plo.data(),
                    m_band_cache_.phi.data(), n);
            }
            if (m_show_food_ && !m_band_cache_.flo.empty())
            {
                ImPlot::SetNextFillStyle({ 0.3f, 1.f, 0.4f, 1.f }, 0.12f);
                ImPlot::PlotShaded("##fb", t, m_band_cache_.flo.data(),
                    m_band_cache_.fhi.data(), n);
            }
        }

        if (m_show_protozoa_)
        {
            ImPlot::SetNextLineStyle({ 0.3f, 0.6f, 1.f, 1.f });
            ImPlot::PlotLine("Protozoa", t, history.protozoa.data(), n);
        }
        if (m_show_food_)
        {
            ImPlot::SetNextLineStyle({ 0.3f, 1.f, 0.4f, 1.f });
            ImPlot::PlotLine("Food", t, history.food.data(), n);
        }
        if (m_show_total_)
        {
            ImPlot::SetNextLineStyle({ 0.65f, 0.65f, 0.65f, 1.f });
            ImPlot::PlotLine("Total", t, history.total.data(), n);
        }

        // Extinction threshold line
        {
            ImPlot::SetNextLineStyle({ 1.f, 0.25f, 0.25f, 0.8f }, 1.f);
            const float tx[2] = { t[0], t[n - 1] };
            const float ty[2] = { 10.f, 10.f };
            ImPlot::PlotLine("##ext", tx, ty, 2);
        }

        // Use the visible-window max for event marker height so they don't
        // blow up the Y axis when hover-paused on historical low-count periods.
        float y_top = 0.f;
        for (int i = 0; i < n; ++i)
            if (history.time[i] >= x_min && history.time[i] <= x_max)
                y_top = std::max(y_top, history.total[i]);
        y_top = std::max(y_top, 10.f); // never shorter than the extinction line

        draw_event_markers(snap, x_min, x_max, y_top);
        if (m_recording_ && m_record_start_ >= x_min)
            draw_record_region(x_max, y_top);
    }

    if (ImPlot::IsPlotHovered() && !m_hover_paused_)
    {
        m_hover_paused_ = true;
        m_paused_x_max_ = live_x;
    }
    else if (!ImPlot::IsPlotHovered())
        m_hover_paused_ = false;

    ImPlot::EndPlot();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Event markers
// ─────────────────────────────────────────────────────────────────────────────
void GraphsTab::draw_event_markers(const SimSnapshot& snap, const float x_min, const float x_max, const float y_top)
{
    const PopulationHistory& history = snap.history;
    for (const auto& ev : history.events)
    {
        if (ev.time < x_min || ev.time > x_max) continue;
        ImPlot::SetNextLineStyle(ev.color);
        const float ex[2] = { ev.time, ev.time };
        const float ey[2] = { 0.f, y_top };
        ImPlot::PlotLine("##ev", ex, ey, 2);

        if (ImPlot::IsPlotHovered())
        {
            const ImPlotPoint mp = ImPlot::GetPlotMousePos();
            if (std::abs(static_cast<float>(mp.x) - ev.time) < 0.8f)
                ImGui::SetTooltip("%s @ %.1fs", ev.label.c_str(), ev.time);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Record region shading
// ─────────────────────────────────────────────────────────────────────────────
void GraphsTab::draw_record_region(const float x_max, const float y_top)
{
    ImPlot::SetNextFillStyle({ 0.4f, 0.8f, 1.f, 1.f }, 0.07f);
    const float rx[2] = { m_record_start_, x_max };
    const float rlo[2] = { 0.f, 0.f };
    const float rhi[2] = { y_top, y_top };
    ImPlot::PlotShaded("##rec", rx, rlo, rhi, 2);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Generations tab
// ─────────────────────────────────────────────────────────────────────────────
void GraphsTab::draw_generations_tab(const SimSnapshot& snap)
{
    const auto& gen_data = snap.stats.gen_data;

    constexpr ImPlotFlags hf = ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMouseText;
    const float           plot_h = ImGui::GetContentRegionAvail().y
        - ImGui::GetFrameHeightWithSpacing() * 2.f;

    if (!gen_data.empty() && ImPlot::BeginPlot("##gen_hist", { -1.f, plot_h }, hf))
    {
        ImPlot::SetupAxes("Generation", "Count",
            ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        ImPlot::SetNextFillStyle({ 0.4f, 0.7f, 1.f, 1.f }, 0.75f);
        ImPlot::PlotHistogram("Generation dist.", gen_data.data(),
            static_cast<int>(gen_data.size()),
            ImPlotBin_Sturges, 1.0, ImPlotRange{});
        ImPlot::EndPlot();
    }
    else if (gen_data.empty())
        ImGui::TextDisabled("No organisms alive.");

    if (!gen_data.empty())
    {
        const float sum = std::accumulate(gen_data.begin(), gen_data.end(), 0.f);
        const float mean = sum / static_cast<float>(gen_data.size());
        const float mx = *std::max_element(gen_data.begin(), gen_data.end());
        ImGui::Separator();
        ImGui::BeginChild("##gstats", { -1.f, ImGui::GetFrameHeightWithSpacing() },
            false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::Text("Mean: %.2f", mean);
        ImGui::SameLine(0, 20); ImGui::Text("Max: %.0f", mx);
        ImGui::SameLine(0, 20); ImGui::Text("N: %d", static_cast<int>(gen_data.size()));
        ImGui::EndChild();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Misc tab
// ─────────────────────────────────────────────────────────────────────────────
void GraphsTab::draw_misc_tab(const SimSnapshot& snap)
{
    const PopulationHistory& history = snap.history;
    // Series toggle row
    ImGui::Checkbox("Mut Rate", &m_show_mut_rate_);    ImGui::SameLine(0, 10);
    ImGui::Checkbox("Mut Range", &m_show_mut_range_);   ImGui::SameLine(0, 10);
    ImGui::Checkbox("Offspring", &m_show_offspring_);
    ImGui::Checkbox("Lifetime", &m_show_lifetime_);    ImGui::SameLine(0, 10);
    ImGui::Checkbox("Avg Cells", &m_show_avg_cells_);   ImGui::SameLine(0, 10);
    ImGui::Checkbox("Avg Springs", &m_show_avg_springs_); ImGui::SameLine(0, 10);
    ImGui::Checkbox("Avg Energy", &m_show_avg_energy_);

    ImGui::SameLine(0, 20);
    if (ImGui::Button("Fit Y##misc"))
        m_refit_misc_ = true;
    ImGui::SameLine();
    ImGui::TextDisabled("(or double-click plot)");

    // Guard: need matching time and misc series
    const MiscSeries& ms = history.misc;
    const int         n = static_cast<int>(std::min(history.time.size(), ms.size()));
    if (n == 0)
    {
        ImGui::TextDisabled("No misc data — enable Track Stats and wait.");
        return;
    }

    const float live_x = snap.stats.iterations_;
    const float x_max = m_hover_paused_ ? m_paused_x_max_ : live_x;
    const float x_min = x_max - m_scroll_window_;
    const float* t = history.time.data();

    // Determine which axes are actually in use this frame.
    const bool any_y1 = m_show_mut_rate_ || m_show_mut_range_;
    const bool any_y2 = m_show_offspring_ || m_show_lifetime_ ||
        m_show_avg_cells_ || m_show_avg_springs_ || m_show_avg_energy_;

    auto compute_axis_range = [&](const std::vector<const std::vector<float>*>& series,
        float& lo, float& hi)
        {
            lo = FLT_MAX;
            hi = -FLT_MAX;

            for (const auto* s : series)
            {
                if (!s) continue; // safety

                float slo, shi;
                if (visible_range(history.time, *s, x_min, x_max, slo, shi))
                {
                    lo = std::min(lo, slo);
                    hi = std::max(hi, shi);
                }
            }

            if (lo == FLT_MAX)
            {
                lo = 0.f;
                hi = 1.f;
            }
        };

    float y1_lo, y1_hi, y2_lo, y2_hi;

    if (any_y1)
    {
        std::vector<const std::vector<float>*> active_y1;
        if (m_show_mut_rate_)  active_y1.push_back(&ms.mut_rate);
        if (m_show_mut_range_) active_y1.push_back(&ms.mut_range);
        
        compute_axis_range(active_y1, y1_lo, y1_hi);
    }
    if (any_y2)
    {
        std::vector<const std::vector<float>*> active_y2;
        if (m_show_offspring_)   active_y2.push_back(&ms.avg_offspring);
        if (m_show_lifetime_)    active_y2.push_back(&ms.avg_lifetime);
        if (m_show_avg_cells_)   active_y2.push_back(&ms.avg_cells);
        if (m_show_avg_springs_) active_y2.push_back(&ms.avg_springs);
        if (m_show_avg_energy_)  active_y2.push_back(&ms.avg_energy);
        compute_axis_range(active_y2, y2_lo, y2_hi);
    }

    constexpr ImPlotFlags pf = ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMouseText;
    if (!ImPlot::BeginPlot("##misc", { -1.f, -1.f }, pf)) return;

    // Set up only the axes that have active series, using per-axis ranges.
    const ImGuiCond fit_cond = m_refit_misc_ ? ImGuiCond_Always : ImGuiCond_Once;

    ImPlot::SetupAxes("Frames",
        any_y1 ? "Rate [0, 1]" : nullptr,
        ImPlotAxisFlags_None,
        any_y1 ? ImPlotAxisFlags_None : ImPlotAxisFlags_NoDecorations);

    ImPlot::SetupAxisLimits(ImAxis_X1, x_min, x_max, ImGuiCond_Always);
    if (any_y1) ImPlot::SetupAxisLimits(ImAxis_Y1, y1_lo, y1_hi, fit_cond);

    if (any_y2)
    {
        ImPlot::SetupAxis(ImAxis_Y2, "Count / Value",
            ImPlotAxisFlags_Opposite);
        ImPlot::SetupAxisLimits(ImAxis_Y2, y2_lo, y2_hi, fit_cond);
    }

    m_refit_misc_ = false;

    // Helpers to plot on the correct axis.
    auto plot_y1 = [&](const char* name, const float* data, const ImVec4 col)
        {
            ImPlot::SetAxes(ImAxis_X1, ImAxis_Y1);
            ImPlot::SetNextLineStyle(col);
            ImPlot::PlotLine(name, t, data, n);
        };
    auto plot_y2 = [&](const char* name, const float* data, const ImVec4 col)
        {
            ImPlot::SetAxes(ImAxis_X1, ImAxis_Y2);
            ImPlot::SetNextLineStyle(col);
            ImPlot::PlotLine(name, t, data, n);
        };

    if (m_show_mut_rate_)    plot_y1("Mut Rate", ms.mut_rate.data(), { 1.f,  0.5f, 0.2f, 1.f });
    if (m_show_mut_range_)   plot_y1("Mut Range", ms.mut_range.data(), { 1.f,  0.8f, 0.2f, 1.f });
    if (m_show_offspring_)   plot_y2("Offspring", ms.avg_offspring.data(), { 0.4f, 1.f,  0.6f, 1.f });
    if (m_show_lifetime_)    plot_y2("Lifetime", ms.avg_lifetime.data(), { 0.6f, 0.4f, 1.f,  1.f });
    if (m_show_avg_cells_)   plot_y2("Avg Cells", ms.avg_cells.data(), { 0.3f, 0.8f, 1.f,  1.f });
    if (m_show_avg_springs_) plot_y2("Avg Springs", ms.avg_springs.data(), { 0.9f, 0.3f, 0.9f, 1.f });
    if (m_show_avg_energy_)  plot_y2("Avg Energy", ms.avg_energy.data(), { 1.f,  0.9f, 0.3f, 1.f });

    ImPlot::EndPlot();
}