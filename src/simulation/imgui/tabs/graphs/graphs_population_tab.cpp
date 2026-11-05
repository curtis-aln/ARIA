#include "graphs_tab.h"
#include "graphs_tab_common.h"
#include "population_history.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <imgui.h>
#include <implot.h>

using namespace GraphsUi;

void GraphsTab::draw_population_tab(const SimSnapshot& snap)
{
    const PopulationHistory& history = snap.history;

    // ── Toggle chips ──────────────────────────────────────────────────────
    chip("Protozoa", &m_show_protozoa_, kProto);        ImGui::SameLine();
    chip("Food", &m_show_food_, kFood);                 ImGui::SameLine();
    chip("Total", &m_show_total_, kTotal);              ImGui::SameLine();
    chip("Cell matter", &m_show_cell_matter_, kMatter); ImGui::SameLine();
    chip("Springs", &m_show_springs_, kSprings);        ImGui::SameLine(0, 24);
    chip("Fill", &m_show_fill_, kNeutral);              ImGui::SameLine();
    chip("Bands", &m_show_bands_, kNeutral);            ImGui::SameLine();
    chip("Events", &m_show_events_, kNeutral);

    // ── Window ────────────────────────────────────────────────────────────
    const float live_x = static_cast<float>(history.current_frame());
    float x_max = m_hover_paused_ ? m_paused_x_max_ : live_x;
    float x_min = x_max - m_scroll_window_;
    if (x_min < 0.f) { x_min = 0.f; x_max = m_scroll_window_; } // no negative iterations on the axis
    const float x_data_end = std::min(x_max, live_x);

    if (m_show_bands_)
        m_band_cache_.refresh(history, m_show_protozoa_, m_show_food_);

    // ── Style ─────────────────────────────────────────────────────────────
    ImPlot::PushStyleColor(ImPlotCol_FrameBg, { 0.f, 0.f, 0.f, 0.f });
    ImPlot::PushStyleColor(ImPlotCol_PlotBg, { 0.f, 0.f, 0.f, 0.f });
    ImPlot::PushStyleColor(ImPlotCol_PlotBorder, { 0.f, 0.f, 0.f, 0.f });
    ImPlot::PushStyleColor(ImPlotCol_AxisGrid, { 1.f, 1.f, 1.f, 0.07f });
    ImPlot::PushStyleColor(ImPlotCol_AxisText, { 0.55f, 0.58f, 0.65f, 1.f });
    ImPlot::PushStyleVar(ImPlotStyleVar_PlotBorderSize, 0.f);
    ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.f);

    constexpr ImPlotFlags pf = ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect |
        ImPlotFlags_NoMouseText | ImPlotFlags_NoLegend;

    if (ImPlot::BeginPlot("##pop", { -1.f, -1.f }, pf))
    {
        const bool any_y2 = m_show_cell_matter_ || m_show_springs_;

        ImPlot::SetupAxes(nullptr, nullptr,
            ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoTickMarks,
            ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoTickMarks);
        if (any_y2) // matter/springs have a different scale, so they get their own axis
            ImPlot::SetupAxis(ImAxis_Y2, nullptr, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_Opposite |
                ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoTickMarks);
        ImPlot::SetupAxisLimits(ImAxis_X1, x_min, x_max, ImGuiCond_Always);

        size_t begin = 0, count = 0;
        history.window_bounds(x_min, x_max, begin, count);
        const int n = static_cast<int>(count);

        if (n > 0)
        {
            const float* t = history.time.data() + begin;

            if (m_show_bands_ && m_band_cache_.valid_for_n == history.size())
            {
                if (m_show_protozoa_ && !m_band_cache_.plo.empty())
                {
                    ImPlot::SetNextFillStyle(kProto, 0.15f);
                    ImPlot::PlotShaded("##pb", t, m_band_cache_.plo.data() + begin,
                        m_band_cache_.phi.data() + begin, n);
                }
                if (m_show_food_ && !m_band_cache_.flo.empty())
                {
                    ImPlot::SetNextFillStyle(kFood, 0.12f);
                    ImPlot::PlotShaded("##fb", t, m_band_cache_.flo.data() + begin,
                        m_band_cache_.fhi.data() + begin, n);
                }
            }

            auto plot_series = [&](const char* name, const std::vector<float>& ys,
                ImVec4 col, ImAxis yax, bool fill)
                {
                    ImPlot::SetAxes(ImAxis_X1, yax);
                    const float* y = ys.data() + begin;
                    if (fill && m_show_fill_ && !m_show_bands_)
                    {
                        char id[48]; snprintf(id, sizeof id, "##fill_%s", name);
                        ImPlot::SetNextFillStyle(col, 0.10f);
                        ImPlot::PlotShaded(id, t, y, n, 0.0);
                    }
                    ImPlot::SetNextLineStyle(col, 2.f);
                    ImPlot::PlotLine(name, t, y, n);
                };

            if (m_show_total_)       plot_series("Total", history.total, kTotal, ImAxis_Y1, false);
            if (m_show_food_)        plot_series("Food", history.food, kFood, ImAxis_Y1, true);
            if (m_show_protozoa_)    plot_series("Protozoa", history.protozoa, kProto, ImAxis_Y1, true);
            if (m_show_cell_matter_) plot_series("Cell matter", history.cell_matter, kMatter, ImAxis_Y2, false);
            if (m_show_springs_)     plot_series("Springs", history.springs, kSprings, ImAxis_Y2, false);

            ImPlot::SetAxes(ImAxis_X1, ImAxis_Y1);

            // Extinction threshold
            {
                ImPlot::SetNextLineStyle({ 1.f, 0.25f, 0.25f, 0.5f }, 1.f);
                const float tx[2] = { t[0], t[n - 1] };
                const float ty[2] = { k_extinction_threshold, k_extinction_threshold };
                ImPlot::PlotLine("##ext", tx, ty, 2);
            }

            float y_top = k_extinction_threshold;
            for (int i = 0; i < n; ++i)
                y_top = std::max(y_top, history.total[begin + i]);

            if (m_show_events_)
                draw_event_markers(snap, x_min, x_max, y_top);
            if (m_recording_ && m_record_start_ >= x_min)
                draw_record_region(x_data_end, y_top);

            // ── Hover readout: faint cursor + values at nearest sample ──────
            if (ImPlot::IsPlotHovered())
            {
                const float mx = static_cast<float>(ImPlot::GetPlotMousePos().x);
                const float* tb = history.time.data() + begin;
                size_t k = std::lower_bound(tb, tb + n, mx) - tb;
                if (k >= count) k = count - 1;
                else if (k > 0 && (mx - tb[k - 1]) < (tb[k] - mx)) --k;
                const size_t i = begin + k;

                double cx = history.time[i];
                ImPlot::SetNextLineStyle({ 1.f, 1.f, 1.f, 0.25f }, 1.f);
                ImPlot::PlotInfLines("##cursor", &cx, 1);

                ImGui::BeginTooltip();
                ImGui::TextDisabled("Frame %.0f", history.time[i]);
                auto row = [](const char* l, ImVec4 c, float v) { ImGui::TextColored(c, "%-12s %.0f", l, v); };
                if (m_show_protozoa_)    row("Protozoa", kProto, history.protozoa[i]);
                if (m_show_food_)        row("Food", kFood, history.food[i]);
                if (m_show_total_)       row("Total", kTotal, history.total[i]);
                if (m_show_cell_matter_) row("Cell matter", kMatter, history.cell_matter[i]);
                if (m_show_springs_)     row("Springs", kSprings, history.springs[i]);
                ImGui::EndTooltip();
            }
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

    ImPlot::PopStyleVar(2);
    ImPlot::PopStyleColor(5);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Event markers
// ─────────────────────────────────────────────────────────────────────────────
void GraphsTab::draw_event_markers(const SimSnapshot& snap, const float x_min, const float x_max, const float y_top)
{
    const PopulationHistory& history = snap.history;
    const float tolerance = (x_max - x_min) * k_event_hit_tolerance_frac;
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
            if (std::abs(static_cast<float>(mp.x) - ev.time) < tolerance)
                ImGui::SetTooltip("%s @ frame %.0f", ev.label.c_str(), ev.time);
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