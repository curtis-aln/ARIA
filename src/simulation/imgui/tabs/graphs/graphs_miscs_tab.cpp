#include "graphs_tab.h"
#include "graphs_tab_common.h"
#include "population_history.h"
#include <imgui.h>
#include <implot.h>

using namespace GraphsUi;

void GraphsTab::draw_misc_tab(const SimSnapshot& snap)
{
    const PopulationHistory& h = snap.history;
    const MiscSeries& ms = h.misc;

    chip("Mut rate", &m_show_mut_rate_, kMutRate);        ImGui::SameLine();
    chip("Mut range", &m_show_mut_range_, kMutRange);     ImGui::SameLine();
    chip("Offspring", &m_show_offspring_, kOffspring);    ImGui::SameLine();
    chip("Lifetime", &m_show_lifetime_, kLifetime);       ImGui::SameLine();
    chip("Avg cells", &m_show_avg_cells_, kAvgCells);     ImGui::SameLine();
    chip("Avg springs", &m_show_avg_springs_, kAvgSpring); ImGui::SameLine();
    chip("Avg energy", &m_show_avg_energy_, kAvgEnergy);

    if (h.size() == 0 || ms.size() == 0)
    {
        ImGui::TextDisabled("No misc data — enable Track Stats and wait.");
        return;
    }

    const TimeWindow win = plot_window(h, m_scroll_window_, m_hover_paused_, m_paused_x_max_);
    size_t begin = 0, count = 0;
    h.window_bounds(win.x_min, win.x_max, begin, count);
    const int n = static_cast<int>(count);
    if (n == 0) { ImGui::TextDisabled("Nothing in the visible window yet."); return; }
    const float* t = h.time.data() + begin;

    const bool any_y1 = m_show_mut_rate_ || m_show_mut_range_;
    const bool any_y2 = m_show_offspring_ || m_show_lifetime_ || m_show_avg_cells_ ||
        m_show_avg_springs_ || m_show_avg_energy_;

    push_plot_style();
    if (ImPlot::BeginPlot("##misc", { -1.f, -1.f }, kPlotFlags))
    {
        // Y1 = rates [0,1]. Y2 = counts/values, on their own scale.
        ImPlotAxisFlags y1f = kYAxis;
        if (!any_y1) y1f |= ImPlotAxisFlags_NoDecorations;

        ImPlot::SetupAxis(ImAxis_X1, nullptr, kXAxis);
        ImPlot::SetupAxis(ImAxis_Y1, nullptr, y1f);
        if (any_y2)
            ImPlot::SetupAxis(ImAxis_Y2, nullptr, kYAxis | ImPlotAxisFlags_Opposite |
                (any_y1 ? ImPlotAxisFlags_NoGridLines : 0)); // one set of grid lines
        ImPlot::SetupAxisLimits(ImAxis_X1, win.x_min, win.x_max, ImGuiCond_Always);

        auto plot = [&](ImAxis yax, const char* name, const std::vector<float>& ys, ImVec4 col)
            {
                ImPlot::SetAxes(ImAxis_X1, yax);
                ImPlot::SetNextLineStyle(col, 2.f);
                ImPlot::PlotLine(name, t, ys.data() + begin, n);
            };

        if (m_show_mut_rate_)    plot(ImAxis_Y1, "Mut rate", ms.mut_rate, kMutRate);
        if (m_show_mut_range_)   plot(ImAxis_Y1, "Mut range", ms.mut_range, kMutRange);
        if (m_show_offspring_)   plot(ImAxis_Y2, "Offspring", ms.avg_offspring, kOffspring);
        if (m_show_lifetime_)    plot(ImAxis_Y2, "Lifetime", ms.avg_lifetime, kLifetime);
        if (m_show_avg_cells_)   plot(ImAxis_Y2, "Avg cells", ms.avg_cells, kAvgCells);
        if (m_show_avg_springs_) plot(ImAxis_Y2, "Avg springs", ms.avg_springs, kAvgSpring);
        if (m_show_avg_energy_)  plot(ImAxis_Y2, "Avg energy", ms.avg_energy, kAvgEnergy);

        ImPlot::SetAxes(ImAxis_X1, ImAxis_Y1);

        if (ImPlot::IsPlotHovered())
        {
            const size_t i = nearest_sample(h.time, begin, count,
                static_cast<float>(ImPlot::GetPlotMousePos().x));
            draw_cursor_line(h.time[i]);

            auto row = [](const char* l, ImVec4 c, float v, int dp)
                { ImGui::TextColored(c, "%-12s %.*f", l, dp, v); };

            ImGui::BeginTooltip();
            ImGui::TextDisabled("Frame %.0f", h.time[i]);
            if (m_show_mut_rate_)    row("Mut rate", kMutRate, ms.mut_rate[i], 4);
            if (m_show_mut_range_)   row("Mut range", kMutRange, ms.mut_range[i], 4);
            if (m_show_offspring_)   row("Offspring", kOffspring, ms.avg_offspring[i], 2);
            if (m_show_lifetime_)    row("Lifetime", kLifetime, ms.avg_lifetime[i], 1);
            if (m_show_avg_cells_)   row("Avg cells", kAvgCells, ms.avg_cells[i], 2);
            if (m_show_avg_springs_) row("Avg springs", kAvgSpring, ms.avg_springs[i], 2);
            if (m_show_avg_energy_)  row("Avg energy", kAvgEnergy, ms.avg_energy[i], 1);
            ImGui::EndTooltip();
        }

        update_hover_pause(m_hover_paused_, m_paused_x_max_, win.live_x);
        ImPlot::EndPlot();
    }
    pop_plot_style();
}