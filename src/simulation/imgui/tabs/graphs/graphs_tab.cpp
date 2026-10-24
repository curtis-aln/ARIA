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
    StatRow::draw("cekks", "%d", p);
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

// ─────────────────────────────────────────────────────────────────────────────
//  Generations tab
// ─────────────────────────────────────────────────────────────────────────────
void GraphsTab::draw_generations_tab(const SimSnapshot& snap)
{
    const auto& gen_data = snap.world_stats.gen_data;

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

    // Guard: need matching time and misc series (these now stay aligned
    // automatically since PopulationHistory::push() writes both together).
    const MiscSeries& ms = history.misc;
    if (history.size() == 0 || ms.size() == 0)
    {
        ImGui::TextDisabled("No misc data — enable Track Stats and wait.");
        return;
    }

    const float live_x = static_cast<float>(history.current_frame());
    const float x_max = m_hover_paused_ ? m_paused_x_max_ : live_x;
    const float x_min = x_max - m_scroll_window_;

    // Only the visible slice ever gets handed to ImPlot, same as the
    // population tab — bounds vertex count regardless of sim runtime.
    size_t begin = 0, count = 0;
    history.window_bounds(x_min, x_max, begin, count);
    const int n = static_cast<int>(count);
    if (n == 0)
    {
        ImGui::TextDisabled("Nothing in the visible window yet.");
        return;
    }
    const float* t = history.time.data() + begin;

    // Determine which axes are actually in use this frame.
    const bool any_y1 = m_show_mut_rate_ || m_show_mut_range_;
    const bool any_y2 = m_show_offspring_ || m_show_lifetime_ ||
        m_show_avg_cells_ || m_show_avg_springs_ || m_show_avg_energy_;

    // Min/max over just the windowed slice — no per-series full-array scan.
    auto compute_axis_range = [&](const std::vector<const std::vector<float>*>& series,
        float& lo, float& hi)
        {
            lo = FLT_MAX;
            hi = -FLT_MAX;

            for (const auto* s : series)
            {
                if (!s) continue; // safety
                for (int i = 0; i < n; ++i)
                {
                    lo = std::min(lo, (*s)[begin + i]);
                    hi = std::max(hi, (*s)[begin + i]);
                }
            }

            if (lo == FLT_MAX)
            {
                lo = 0.f;
                hi = 1.f;
            }
            else
            {
                // Pad so the line doesn't kiss the axis edges.
                const float pad = std::max((hi - lo) * 0.12f, 0.01f);
                lo -= pad;
                hi += pad;
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

    if (m_show_mut_rate_)    plot_y1("Mut Rate", ms.mut_rate.data() + begin, { 1.f,  0.5f, 0.2f, 1.f });
    if (m_show_mut_range_)   plot_y1("Mut Range", ms.mut_range.data() + begin, { 1.f,  0.8f, 0.2f, 1.f });
    if (m_show_offspring_)   plot_y2("Offspring", ms.avg_offspring.data() + begin, { 0.4f, 1.f,  0.6f, 1.f });
    if (m_show_lifetime_)    plot_y2("Lifetime", ms.avg_lifetime.data() + begin, { 0.6f, 0.4f, 1.f,  1.f });
    if (m_show_avg_cells_)   plot_y2("Avg Cells", ms.avg_cells.data() + begin, { 0.3f, 0.8f, 1.f,  1.f });
    if (m_show_avg_springs_) plot_y2("Avg Springs", ms.avg_springs.data() + begin, { 0.9f, 0.3f, 0.9f, 1.f });
    if (m_show_avg_energy_)  plot_y2("Avg Energy", ms.avg_energy.data() + begin, { 1.f,  0.9f, 0.3f, 1.f });

    ImPlot::EndPlot();
}