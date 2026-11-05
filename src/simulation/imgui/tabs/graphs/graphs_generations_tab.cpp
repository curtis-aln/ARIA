#include "graphs_tab.h"
#include "graphs_tab_common.h"
#include "population_history.h"
#include <algorithm>
#include <imgui.h>
#include <implot.h>

using namespace GraphsUi;

void GraphsTab::draw_generations_tab(const SimSnapshot& snap)
{
    const PopulationHistory& h = snap.history;

    chip("Average", &m_show_gen_avg_, kGen);       ImGui::SameLine();
    chip("Min / max", &m_show_gen_range_, kGen);   ImGui::SameLine();
    chip("Distribution", &m_show_gen_dist_, kNeutral);

    if (h.size() == 0) { ImGui::TextDisabled("No generation data yet."); return; }

    ImGui::SameLine(0, 24);
    ImGui::TextDisabled("Avg");   ImGui::SameLine(); ImGui::Text("%.2f", h.avg_gen.back());  ImGui::SameLine(0, 14);
    ImGui::TextDisabled("Min");   ImGui::SameLine(); ImGui::Text("%.0f", h.min_gen.back());  ImGui::SameLine(0, 14);
    ImGui::TextDisabled("Max");   ImGui::SameLine(); ImGui::Text("%.0f", h.max_gen.back());  ImGui::SameLine(0, 14);
    ImGui::TextDisabled("Alive"); ImGui::SameLine(); ImGui::Text("%d", static_cast<int>(h.gen_distribution.size()));

    const TimeWindow win = plot_window(h, m_scroll_window_, m_hover_paused_, m_paused_x_max_);
    size_t begin = 0, count = 0;
    h.window_bounds(win.x_min, win.x_max, begin, count);
    const int n = static_cast<int>(count);

    const bool show_dist = m_show_gen_dist_ && !h.gen_distribution.empty();
    const float time_w = show_dist
        ? (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.62f
        : -1.f;

    push_plot_style();

    // ── Generation over time ──────────────────────────────────────────────
    if (ImPlot::BeginPlot("##gen_time", { time_w, -1.f }, kPlotFlags))
    {
        ImPlot::SetupAxes(nullptr, nullptr, kXAxis, kYAxis);
        ImPlot::SetupAxisLimits(ImAxis_X1, win.x_min, win.x_max, ImGuiCond_Always);

        if (n > 0)
        {
            const float* t = h.time.data() + begin;

            if (m_show_gen_range_)
            {
                ImPlot::SetNextFillStyle(kGen, 0.14f);
                ImPlot::PlotShaded("##gen_rng", t, h.min_gen.data() + begin, h.max_gen.data() + begin, n);
            }
            if (m_show_gen_avg_)
            {
                ImPlot::SetNextLineStyle(kGen, 2.f);
                ImPlot::PlotLine("Avg generation", t, h.avg_gen.data() + begin, n);
            }

            if (ImPlot::IsPlotHovered())
            {
                const size_t i = nearest_sample(h.time, begin, count,
                    static_cast<float>(ImPlot::GetPlotMousePos().x));
                draw_cursor_line(h.time[i]);

                ImGui::BeginTooltip();
                ImGui::TextDisabled("Frame %.0f", h.time[i]);
                ImGui::TextColored(kGen, "%-12s %.2f", "Average", h.avg_gen[i]);
                ImGui::TextColored(kGen, "%-12s %.0f", "Min", h.min_gen[i]);
                ImGui::TextColored(kGen, "%-12s %.0f", "Max", h.max_gen[i]);
                ImGui::EndTooltip();
            }
        }

        update_hover_pause(m_hover_paused_, m_paused_x_max_, win.live_x);
        ImPlot::EndPlot();
    }

    // ── Distribution at the latest sample ─────────────────────────────────
    if (show_dist)
    {
        ImGui::SameLine();
        if (ImPlot::BeginPlot("##gen_dist", { -1.f, -1.f }, kPlotFlags))
        {
            const auto& d = h.gen_distribution;
            const auto [lo_it, hi_it] = std::minmax_element(d.begin(), d.end());
            const double lo = *lo_it, hi = *hi_it;
            // one bin per integer generation, capped so huge spreads stay readable
            const int bins = std::clamp(static_cast<int>(hi - lo) + 1, 1, 64);

            ImPlot::SetupAxes("Generation", nullptr, kXAxis | ImPlotAxisFlags_AutoFit, kYAxis);
            ImPlot::SetNextFillStyle(kGen, 0.55f);
            ImPlot::PlotHistogram("Distribution", d.data(), static_cast<int>(d.size()),
                bins, 1.0, ImPlotRange(lo - 0.5, hi + 0.5));
            ImPlot::EndPlot();
        }
    }

    pop_plot_style();
}