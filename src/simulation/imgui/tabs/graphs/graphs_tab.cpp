#include "graphs_tab.h"
#include "population_history.h"
#include <imgui.h>
#include <implot.h>
#include <numeric>
#include <algorithm>
#include <cfloat>
#include "stat_row.h"

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
    if (!ImGui::BeginTabBar("##graph_tabs")) return;

    if (ImGui::BeginTabItem("Population")) { draw_population_tab(snap);  ImGui::EndTabItem(); }
    if (ImGui::BeginTabItem("Generations")) { draw_generations_tab(snap); ImGui::EndTabItem(); }
    if (ImGui::BeginTabItem("Misc")) { draw_misc_tab(snap);        ImGui::EndTabItem(); }

    ImGui::EndTabBar();

    ImGui::EndChild();
}

void GraphsTab::draw_stat_panels(const SimSnapshot& snap)
{
    const float avail_h = ImGui::GetContentRegionAvail().y;
    const float sp = ImGui::GetStyle().ItemSpacing.y;
    const float panel_h = (avail_h);

    // ── Population ────────────────────────────────────────────────────────────
    ImGui::BeginChild("ST_pop", { -1.f, panel_h }, true);
    ImGui::TextDisabled("Population");
    ImGui::Separator();
    const int  p = snap.cell_manager_stats.cell_count;
    const int  f = snap.food_manager_stats.food_count;
    const bool risk = p <= 10;
    StatRow::draw("Cells", "%d", p);
    StatRow::draw("Food", "%d", f);
	StatRow::draw("Matter", "%d", snap.cell_manager_stats.cell_matter_count);
	StatRow::draw("Springs", "%d", snap.cell_manager_stats.spring_count);
    StatRow::draw("Total", "%d", p + f);
    StatRow::draw_warn("Ext. risk", risk, "%s", risk ? "YES" : "no");
    StatRow::draw("Peak ever", "%d", snap.cell_manager_stats.peak_protozoa_ever);

    // ── Vitals ────────────────────────────────────────────────────────────────
    ImGui::TextDisabled("Vitals");
    ImGui::Separator();
    StatRow::draw("Avg lifetime", "%.1f fr", snap.cell_manager_stats.average_lifetime);
    StatRow::draw("Longest Lifetime", "%d fr", snap.cell_manager_stats.longest_lived_ever);
    StatRow::draw("Births /100f", "%.1f", snap.cell_manager_stats.births_per_hundered_frames);
    StatRow::draw("Deaths /100f", "%.1f", snap.cell_manager_stats.deaths_per_hundered_frames);
    StatRow::draw("Non-repro. deaths /100f", "%.1f", snap.cell_manager_stats.non_repro_deaths_per_hundered_frames);
    StatRow::draw("Infant mortality", "%.1f%%", snap.cell_manager_stats.infant_mortality_rate * 100.f);

    // ── Genetics ──────────────────────────────────────────────────────────────
    ImGui::TextDisabled("Genetics");
    ImGui::Separator();
    StatRow::draw("Avg generation", "%.2f", snap.cell_manager_stats.average_generation);
    StatRow::draw("Highest gen ever", "%d", snap.cell_manager_stats.highest_generation_ever);
    StatRow::draw("Most offspring", "%d", snap.cell_manager_stats.most_offspring_ever);
    StatRow::draw("Frames / gen", "%.0f", snap.world_stats.frames_per_generation);
    StatRow::draw("Avg mut rate", "%.4f", snap.cell_manager_stats.average_mutation_rate);
    StatRow::draw("Avg mut range", "%.4f", snap.cell_manager_stats.average_mutation_range);

    // ── Morphology ────────────────────────────────────────────────────────────
    ImGui::TextDisabled("Morphology");
    ImGui::Separator();
    StatRow::draw("Avg cells", "%.2f", snap.cell_manager_stats.average_cells_per_protozoa);
    StatRow::draw("Avg springs", "%.2f", snap.cell_manager_stats.average_spring_count);
    StatRow::draw("Avg offspring", "%.2f", snap.cell_manager_stats.average_offspring_count);
    StatRow::draw("Avg energy", "%.1f", snap.cell_manager_stats.average_energy);
    StatRow::draw("Energy ratio", "%.3f", snap.cell_manager_stats.energy_efficiency);
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


// ─────────────────────────────────────────────────────────────────────────────
//  Population tab
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    constexpr ImVec4 kProto = { 0.30f, 0.62f, 1.00f, 1.f };
    constexpr ImVec4 kFood = { 0.30f, 0.90f, 0.50f, 1.f };
    constexpr ImVec4 kTotal = { 0.72f, 0.74f, 0.80f, 1.f };
    constexpr ImVec4 kMatter = { 1.00f, 0.65f, 0.25f, 1.f };
    constexpr ImVec4 kSprings = { 0.85f, 0.45f, 1.00f, 1.f };
    constexpr ImVec4 kNeutral = { 0.60f, 0.65f, 0.75f, 1.f };

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

    constexpr ImVec4 kGen = { 0.40f, 0.85f, 0.95f, 1.f };
    constexpr ImVec4 kMutRate = { 1.00f, 0.50f, 0.20f, 1.f };
    constexpr ImVec4 kMutRange = { 1.00f, 0.80f, 0.20f, 1.f };
    constexpr ImVec4 kOffspring = { 0.40f, 1.00f, 0.60f, 1.f };
    constexpr ImVec4 kLifetime = { 0.60f, 0.40f, 1.00f, 1.f };
    constexpr ImVec4 kAvgCells = { 0.30f, 0.80f, 1.00f, 1.f };
    constexpr ImVec4 kAvgSpring = { 0.90f, 0.30f, 0.90f, 1.f };
    constexpr ImVec4 kAvgEnergy = { 1.00f, 0.90f, 0.30f, 1.f };

    constexpr ImPlotFlags     kPlotFlags = ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect |
        ImPlotFlags_NoMouseText | ImPlotFlags_NoLegend;
    constexpr ImPlotAxisFlags kXAxis = ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoTickMarks;
    constexpr ImPlotAxisFlags kYAxis = ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoTickMarks;

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
    void pop_plot_style() { ImPlot::PopStyleVar(3); ImPlot::PopStyleColor(5); }

    struct TimeWindow { float x_min, x_max, live_x; };

    TimeWindow plot_window(const PopulationHistory& h, float span, bool hover_paused, float paused_x_max)
    {
        const float live = static_cast<float>(h.current_frame());
        float hi = hover_paused ? paused_x_max : live;
        float lo = hi - span;
        if (lo < 0.f) { lo = 0.f; hi = span; }
        return { lo, hi, live };
    }

    // Call inside BeginPlot. Freezes the window while the mouse is over the plot.
    void update_hover_pause(bool& paused, float& paused_x_max, float live_x)
    {
        if (ImPlot::IsPlotHovered()) { if (!paused) { paused = true; paused_x_max = live_x; } }
        else paused = false;
    }

    // Absolute index of the sample nearest to mx. Requires count > 0.
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