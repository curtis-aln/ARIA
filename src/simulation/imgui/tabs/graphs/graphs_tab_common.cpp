#include "graphs_tab_common.h"
#include <algorithm>

namespace GraphsUi
{
    void chip(const char* label, bool* v, ImVec4 col)
    {
        const bool on = *v;
        const ImVec4 bg = on ? ImVec4{ col.x, col.y, col.z, 0.20f } : ImVec4{ 1.f, 1.f, 1.f, 0.04f };
        const ImVec4 bgh = on ? ImVec4{ col.x, col.y, col.z, 0.34f } : ImVec4{ 1.f, 1.f, 1.f, 0.10f };
        const ImVec4 txt = on ? col : ImVec4{ 0.50f, 0.53f, 0.60f, 1.f };

        ImGui::PushStyleColor(ImGuiCol_Button, bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bgh);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, bgh);
        ImGui::PushStyleColor(ImGuiCol_Text, txt);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 10.f, 3.f });
        if (ImGui::Button(label)) *v = !*v;
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
    }

    void push_plot_style()
    {
        ImPlot::PushStyleColor(ImPlotCol_FrameBg, { 0.f, 0.f, 0.f, 0.f });
        ImPlot::PushStyleColor(ImPlotCol_PlotBg, { 0.f, 0.f, 0.f, 0.f });
        ImPlot::PushStyleColor(ImPlotCol_PlotBorder, { 0.f, 0.f, 0.f, 0.f });
        ImPlot::PushStyleColor(ImPlotCol_AxisGrid, { 1.f, 1.f, 1.f, 0.07f });
        ImPlot::PushStyleColor(ImPlotCol_AxisText, { 0.55f, 0.58f, 0.65f, 1.f });
        ImPlot::PushStyleVar(ImPlotStyleVar_PlotBorderSize, 0.f);
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.f);
        ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, ImVec2(0.f, 0.1f)); // 10% headroom on auto-fit Y
    }

    void pop_plot_style()
    {
        ImPlot::PopStyleVar(3);
        ImPlot::PopStyleColor(5);
    }

    TimeWindow plot_window(const PopulationHistory& h, float span, bool hover_paused, float paused_x_max)
    {
        const float live = static_cast<float>(h.current_frame());
        float hi = hover_paused ? paused_x_max : live;
        float lo = hi - span;
        if (lo < 0.f) { lo = 0.f; hi = span; }
        return { lo, hi, live };
    }

    void update_hover_pause(bool& paused, float& paused_x_max, float live_x)
    {
        if (ImPlot::IsPlotHovered()) { if (!paused) { paused = true; paused_x_max = live_x; } }
        else paused = false;
    }

    size_t nearest_sample(const std::vector<float>& time, size_t begin, size_t count, float mx)
    {
        const float* tb = time.data() + begin;
        size_t k = static_cast<size_t>(std::lower_bound(tb, tb + count, mx) - tb);
        if (k >= count) k = count - 1;
        else if (k > 0 && (mx - tb[k - 1]) < (tb[k] - mx)) --k;
        return begin + k;
    }

    void draw_cursor_line(double x)
    {
        ImPlot::SetNextLineStyle({ 1.f, 1.f, 1.f, 0.25f }, 1.f);
        ImPlot::PlotInfLines("##cursor", &x, 1);
    }
}