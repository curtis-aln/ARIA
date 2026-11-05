#pragma once
// Internal helpers shared by the graphs_*.cpp files. Not part of GraphsTab's public API.

#include "population_history.h"
#include <cstddef>
#include <imgui.h>
#include <implot.h>
#include <vector>

namespace GraphsUi
{
    // ── Population colours ────────────────────────────────────────────────
    inline constexpr ImVec4 kProto = { 0.30f, 0.62f, 1.00f, 1.f };
    inline constexpr ImVec4 kFood = { 0.30f, 0.90f, 0.50f, 1.f };
    inline constexpr ImVec4 kTotal = { 0.72f, 0.74f, 0.80f, 1.f };
    inline constexpr ImVec4 kMatter = { 1.00f, 0.65f, 0.25f, 1.f };
    inline constexpr ImVec4 kSprings = { 0.85f, 0.45f, 1.00f, 1.f };
    inline constexpr ImVec4 kNeutral = { 0.60f, 0.65f, 0.75f, 1.f };

    // ── Generations / misc colours ────────────────────────────────────────
    inline constexpr ImVec4 kGen = { 0.40f, 0.85f, 0.95f, 1.f };
    inline constexpr ImVec4 kMutRate = { 1.00f, 0.50f, 0.20f, 1.f };
    inline constexpr ImVec4 kMutRange = { 1.00f, 0.80f, 0.20f, 1.f };
    inline constexpr ImVec4 kOffspring = { 0.40f, 1.00f, 0.60f, 1.f };
    inline constexpr ImVec4 kLifetime = { 0.60f, 0.40f, 1.00f, 1.f };
    inline constexpr ImVec4 kAvgCells = { 0.30f, 0.80f, 1.00f, 1.f };
    inline constexpr ImVec4 kAvgSpring = { 0.90f, 0.30f, 0.90f, 1.f };
    inline constexpr ImVec4 kAvgEnergy = { 1.00f, 0.90f, 0.30f, 1.f };

    // ── Shared plot flags ─────────────────────────────────────────────────
    inline constexpr ImPlotFlags     kPlotFlags = ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect |
        ImPlotFlags_NoMouseText | ImPlotFlags_NoLegend;
    inline constexpr ImPlotAxisFlags kXAxis = ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoTickMarks;
    inline constexpr ImPlotAxisFlags kYAxis = ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoTickMarks;

    struct TimeWindow { float x_min, x_max, live_x; };

    // ── Helpers (defined in graphs_tab_common.cpp) ────────────────────────
    void chip(const char* label, bool* v, ImVec4 col);

    void push_plot_style();
    void pop_plot_style();

    TimeWindow plot_window(const PopulationHistory& h, float span, bool hover_paused, float paused_x_max);

    // Call inside BeginPlot. Freezes the window while the mouse is over the plot.
    void update_hover_pause(bool& paused, float& paused_x_max, float live_x);

    // Absolute index of the sample nearest to mx. Requires count > 0.
    size_t nearest_sample(const std::vector<float>& time, size_t begin, size_t count, float mx);

    void draw_cursor_line(double x);
}