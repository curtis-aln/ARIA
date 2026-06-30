#include "o_vector_tab.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <sstream>

// ─────────────────────────────────────────────────────────────────────────────
//  Design constants
// ─────────────────────────────────────────────────────────────────────────────

static constexpr float k_bar_height = 14.f;   // main capacity bars
static constexpr float k_mini_bar_height = 8.f;    // churn heatmap row height
static constexpr float k_heatmap_min_w = 6.f;     // minimum slot cell width in heatmap
static constexpr int   k_history_scroll = 128;     // max history samples kept for sparkline

// Colours reused across the whole tab
static constexpr ImVec4 k_col_cells = { 0.35f, 0.75f, 0.35f, 1.f }; // green
static constexpr ImVec4 k_col_food = { 0.85f, 0.65f, 0.15f, 1.f }; // amber
static constexpr ImVec4 k_col_bodies = { 0.45f, 0.65f, 0.95f, 1.f }; // blue
static constexpr ImVec4 k_col_springs = { 0.80f, 0.40f, 0.75f, 1.f }; // purple
static constexpr ImVec4 k_col_warn = { 1.00f, 0.50f, 0.10f, 1.f }; // orange warning
static constexpr ImVec4 k_col_ok = { 0.40f, 0.90f, 0.40f, 1.f }; // integrity pass
static constexpr ImVec4 k_col_fail = { 0.95f, 0.25f, 0.25f, 1.f }; // integrity fail

// Vector display names and their matching colours — indexed 0-3 everywhere.
static const char* k_vec_names[4] = { "Cells", "Food", "Bodies", "Springs" };
static const ImVec4   k_vec_colours[4] = { k_col_cells, k_col_food, k_col_bodies, k_col_springs };


// ─────────────────────────────────────────────────────────────────────────────
//  Internal helper utilities  (file-local, no header needed)
// ─────────────────────────────────────────────────────────────────────────────

// Horizontal progress bar with a colour override and overlay label.
static void colored_bar(float fraction, ImVec4 color,
    const char* overlay, ImVec2 size = { -1.f, k_bar_height })
{
    fraction = std::clamp(fraction, 0.f, 1.f);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
    ImGui::ProgressBar(fraction, size, overlay);
    ImGui::PopStyleColor();
}

// A bar that goes green→yellow→red as fill_ratio increases: 0=green, 1=red.
static ImVec4 fill_heat_color(float ratio)
{
    ratio = std::clamp(ratio, 0.f, 1.f);
    return ratio > 0.5f
        ? ImVec4{ 1.f,       2.f * (1.f - ratio), 0.1f, 1.f }
    : ImVec4{ 2.f * ratio, 1.f,               0.1f, 1.f };
}

// Fragmentation goes blue (0 = compact) → red (1 = fragmented).
static ImVec4 frag_color(float score)
{
    score = std::clamp(score, 0.f, 1.f);
    return { score, 0.2f, 1.f - score, 1.f };
}

// Formats a byte count as "X B / X KB / X MB".
static void format_bytes(char* buf, int buf_sz, size_t bytes)
{
    if (bytes < 1024)             snprintf(buf, buf_sz, "%zu B", bytes);
    else if (bytes < 1024 * 1024)      snprintf(buf, buf_sz, "%.1f KB", bytes / 1024.f);
    else                               snprintf(buf, buf_sz, "%.2f MB", bytes / (1024.f * 1024.f));
}

// A thin separator line with optional label.
static void section(const char* title)
{
    ImGui::Spacing();
    ImGui::TextDisabled("%s", title);
    ImGui::Separator();
    ImGui::Spacing();
}


// ─────────────────────────────────────────────────────────────────────────────
//  Construction
// ─────────────────────────────────────────────────────────────────────────────

OVecDebugTab::OVecDebugTab()

{
    m_integrity_log_.reserve(32);
}


// ─────────────────────────────────────────────────────────────────────────────
//  Top-level draw  —  builds snapshots then dispatches to sub-tabs
// ─────────────────────────────────────────────────────────────────────────────

void OVecDebugTab::draw(const SimSnapshot& snap, ImGuiContext& ctx)
{
    if (!ImGui::BeginTabBar("##ov_debug_tabs"))
        return;

    if (ImGui::BeginTabItem("Overview")) { draw_overview_tab(snap, ctx);   ImGui::EndTabItem(); }
    if (ImGui::BeginTabItem("Capacity")) { draw_capacity_tab(snap, ctx);   ImGui::EndTabItem(); }
    if (ImGui::BeginTabItem("Churn")) { draw_churn_tab(snap, ctx);      ImGui::EndTabItem(); }
    if (ImGui::BeginTabItem("Snapshots")) { draw_snapshots_tab(snap, ctx);  ImGui::EndTabItem(); }
    if (ImGui::BeginTabItem("Integrity")) { draw_integrity_tab(snap, ctx);  ImGui::EndTabItem(); }

    ImGui::EndTabBar();
}


// ═════════════════════════════════════════════════════════════════════════════
//  SUB-TAB 1 — Overview
//  A quick at-a-glance health summary for all four vectors on one screen.
// ═════════════════════════════════════════════════════════════════════════════

void OVecDebugTab::draw_overview_tab(const SimSnapshot& snap, ImGuiContext& ctx)
{
    // ── Per-vector summary cards ──────────────────────────────────────────────
    section("Live Status");

	std::vector<OVecDebugImGuiSnapshot> o_vector_snapshots = {
		snap.render.cell_debug_snapshot,
		snap.render.food_debug_snapshot,
		snap.render.body_debug_snapshot,
		snap.render.spring_debug_snapshot
	};

    const float card_w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 3.f) / 4.f;
    const float card_h = 130.f;

    for (int i = 0; i < 4; ++i)
    {
        const OVecDebugImGuiSnapshot& s = o_vector_snapshots[i];
        if (i > 0) ImGui::SameLine();

        ImGui::PushID(i);
        ImGui::BeginChild("##ov_card", { card_w, card_h }, true);

        // Header with colour-coded vector name
        ImGui::PushStyleColor(ImGuiCol_Text, s.color);
        ImGui::TextUnformatted(s.name);
        ImGui::PopStyleColor();
        ImGui::Separator();

        // Key numbers
        ImGui::Text("Active  %d", s.active);
        ImGui::Text("Free    %d", s.free_slots);
        ImGui::Text("Size    %d", s.array_size);

        // Fill bar — colour encodes pressure (green=ok, red=full)
        ImGui::Spacing();
        char fill_lbl[24];
        snprintf(fill_lbl, sizeof(fill_lbl), "Fill %.0f%%", s.fill_ratio * 100.f);
        colored_bar(s.fill_ratio, fill_heat_color(s.fill_ratio), fill_lbl);

        // Fragmentation bar — blue=compact, red=fragmented
        char frag_lbl[24];
        snprintf(frag_lbl, sizeof(frag_lbl), "Frag %.0f%%", s.frag_score * 100.f);
        colored_bar(s.frag_score, frag_color(s.frag_score), frag_lbl);

        ImGui::EndChild();
        ImGui::PopID();
    }

    // ── Aggregate totals row ──────────────────────────────────────────────────
    section("Aggregate");

    int   total_active = 0;
    int   total_size = 0;
    float total_wasted_kb = 0.f;
    uint64_t total_ops = 0;

    for (const auto& s : o_vector_snapshots)
    {
        total_active += s.active;
        total_size += s.array_size;
        total_wasted_kb += static_cast<float>(s.wasted_bytes) / 1024.f;
        total_ops += s.total_emplaces + s.total_adds + s.total_removes;
    }

    ImGui::Columns(4, "##agg_cols", false);
    ImGui::Text("Total active:  %d", total_active);  ImGui::NextColumn();
    ImGui::Text("Total slots:   %d", total_size);    ImGui::NextColumn();
    ImGui::Text("Wasted:  %.1f KB", total_wasted_kb); ImGui::NextColumn();
    ImGui::Text("Lifetime ops:  %llu", total_ops);
    ImGui::Columns(1);

    // ── Per-vector operation summary table ────────────────────────────────────
    section("Operations (lifetime)");

    if (ImGui::BeginTable("##ov_ops", 6,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("Vector", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Emplace", ImGuiTableColumnFlags_WidthFixed, 70.f);
        ImGui::TableSetupColumn("Add", ImGuiTableColumnFlags_WidthFixed, 60.f);
        ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed, 70.f);
        ImGui::TableSetupColumn("Fail", ImGuiTableColumnFlags_WidthFixed, 50.f);
        ImGui::TableSetupColumn("Ops/s", ImGuiTableColumnFlags_WidthFixed, 80.f);
        ImGui::TableHeadersRow();

        for (const auto& s : o_vector_snapshots)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, s.color);
            ImGui::TextUnformatted(s.name);
            ImGui::PopStyleColor();

            ImGui::TableSetColumnIndex(1); ImGui::Text("%llu", s.total_emplaces);
            ImGui::TableSetColumnIndex(2); ImGui::Text("%llu", s.total_adds);
            ImGui::TableSetColumnIndex(3); ImGui::Text("%llu", s.total_removes);

            ImGui::TableSetColumnIndex(4);
            if (s.failed_adds > 0)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, k_col_warn);
                ImGui::Text("%llu", s.failed_adds);
                ImGui::PopStyleColor();
            }
            else
            {
                ImGui::TextDisabled("0");
            }

            ImGui::TableSetColumnIndex(5); ImGui::Text("%.0f", s.ops_per_sec);
        }
        ImGui::EndTable();
    }

    // ── Alert banner: any vector at >90% fill or >50% fragmentation ──────────
    for (const auto& s : o_vector_snapshots)
    {
        if (s.fill_ratio > 0.90f)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, k_col_warn);
            ImGui::Text("⚠  %s  is %.0f%% full — consider increasing initial capacity!",
                s.name, s.fill_ratio * 100.f);
            ImGui::PopStyleColor();
        }
        if (s.frag_score > 0.50f)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, k_col_warn);
            ImGui::Text("⚠  %s  fragmentation score %.2f — smart_resize() may help",
                s.name, s.frag_score);
            ImGui::PopStyleColor();
        }
    }
}


// ═════════════════════════════════════════════════════════════════════════════
//  SUB-TAB 2 — Capacity
//  Detailed slot layout, memory, peaks, and resize events per vector.
// ═════════════════════════════════════════════════════════════════════════════

void OVecDebugTab::draw_capacity_tab(const SimSnapshot& snap, ImGuiContext& ctx)
{
    std::vector<OVecDebugImGuiSnapshot> o_vector_snapshots = {
        snap.render.cell_debug_snapshot,
        snap.render.food_debug_snapshot,
        snap.render.body_debug_snapshot,
        snap.render.spring_debug_snapshot
    };

    for (int i = 0; i < 4; ++i)
    {
        const OVecDebugImGuiSnapshot& s = o_vector_snapshots[i];

        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Text, s.color);

        // Collapsing header per vector so the tab isn't overwhelming
        const bool open = ImGui::CollapsingHeader(s.name,
            ImGuiTreeNodeFlags_DefaultOpen);

        ImGui::PopStyleColor();

        if (!open) { ImGui::PopID(); continue; }

        ImGui::Indent();

        // ── Slot counts ───────────────────────────────────────────────────────
        ImGui::Columns(3, "##cap_nums", false);

        ImGui::TextDisabled("Slots");
        ImGui::Text("Active       %d", s.active);
        ImGui::Text("Free         %d", s.free_slots);
        ImGui::Text("Array size   %d", s.array_size);

        ImGui::NextColumn();
        ImGui::TextDisabled("Peaks");
        ImGui::Text("Peak active  %d", s.peak_active);
        ImGui::Text("Peak size    %d", s.peak_array_size);

        ImGui::NextColumn();
        ImGui::TextDisabled("Resize events");
        ImGui::Text("Grows   %llu", s.resize_grows);
        ImGui::Text("Shrinks %llu", s.resize_shrinks);

        ImGui::Columns(1);
        ImGui::Spacing();

        // ── Fill bar ──────────────────────────────────────────────────────────
        char fill_lbl[48];
        snprintf(fill_lbl, sizeof(fill_lbl), "%d / %d  (%.1f%%)",
            s.active, s.array_size, s.fill_ratio * 100.f);
        ImGui::TextDisabled("Fill ratio");
        colored_bar(s.fill_ratio, fill_heat_color(s.fill_ratio), fill_lbl,
            { -1.f, k_bar_height });

        // ── Fragmentation detail ──────────────────────────────────────────────
        ImGui::Spacing();
        ImGui::TextDisabled("Fragmentation");
        char frag_lbl[32];
        snprintf(frag_lbl, sizeof(frag_lbl), "Score  %.3f", s.frag_score);
        colored_bar(s.frag_score, frag_color(s.frag_score), frag_lbl,
            { -1.f, k_bar_height });

        ImGui::Columns(3, "##frag_detail", false);
        ImGui::Text("Holes        %d", s.hole_count);
        ImGui::NextColumn();
        ImGui::Text("Longest hole %d slots", s.longest_hole);
        ImGui::NextColumn();
        ImGui::Text("Longest run  %d slots", s.longest_run);
        ImGui::Columns(1);

        // ── Memory ───────────────────────────────────────────────────────────
        ImGui::Spacing();
        ImGui::TextDisabled("Memory (estimate)");

        char heap_buf[24], waste_buf[24];
        format_bytes(heap_buf, sizeof(heap_buf), s.heap_bytes);
        format_bytes(waste_buf, sizeof(waste_buf), s.wasted_bytes);

        ImGui::Text("Total heap   %s", heap_buf);
        ImGui::Text("Wasted       %s  (%d inactive slots × sizeof(Obj))",
            waste_buf, s.free_slots);

        // Waste bar: fraction of heap that is unused
        const float waste_frac = (s.heap_bytes > 0)
            ? static_cast<float>(s.wasted_bytes) / static_cast<float>(s.heap_bytes)
            : 0.f;
        char waste_lbl[32];
        snprintf(waste_lbl, sizeof(waste_lbl), "Waste %.0f%%", waste_frac * 100.f);
        colored_bar(waste_frac, fill_heat_color(waste_frac), waste_lbl,
            { -1.f, k_bar_height });

        // ── History sparkline (active count over time) ────────────────────────
        //   We draw a mini line chart from OVecDebug::history using PlotLines.
        ImGui::Spacing();
        ImGui::TextDisabled("Active count history  (record_sample() / auto_history)");

        // IMGUI TODO: wire up history[] from the correct OVecDebug instance.
        // Each OVecDebug<Obj> exposes  std::vector<Sample> history.
        // Pull s.active values out into a float buffer and call ImGui::PlotLines.
        // Example skeleton:
        //   auto& h = m_dbg_cells_.history;
        //   std::vector<float> vals(h.size());
        //   for (int j = 0; j < (int)h.size(); ++j) vals[j] = (float)h[j].active;
        //   ImGui::PlotLines("##spark", vals.data(), (int)vals.size(),
        //                    0, nullptr, 0.f, (float)s.peak_active,
        //                    { -1.f, 40.f });
        ImGui::TextDisabled("[sparkline — connect history[] here]");

        ImGui::Unindent();
        ImGui::Spacing();
        ImGui::PopID();
    }
}


// ═════════════════════════════════════════════════════════════════════════════
//  SUB-TAB 3 — Churn
//  Per-slot reuse heatmap and a sortable top-N table of hottest slots.
// ═════════════════════════════════════════════════════════════════════════════

void OVecDebugTab::draw_churn_tab(const SimSnapshot& snap, ImGuiContext& ctx)
{
    // ── Vector selector ───────────────────────────────────────────────────────
    ImGui::TextDisabled("Vector:");
    ImGui::SameLine();
    for (int i = 0; i < 4; ++i)
    {
        if (i > 0) ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, k_vec_colours[i]);
        if (ImGui::RadioButton(k_vec_names[i], m_churn_vec_sel_ == i))
            m_churn_vec_sel_ = i;
        ImGui::PopStyleColor();
    }

    ImGui::SetNextItemWidth(160.f);
    ImGui::SliderInt("Top N", &m_churn_top_n_, 5, 50);

    ImGui::Separator();
    ImGui::Spacing();

    // ── IMGUI TODO: retrieve slot_churn_ for the selected vector ─────────────
    //  slot_churn_ is private in OVecDebug but could be exposed via a
    //  const accessor:  const std::vector<uint32_t>& slot_churn() const;
    //  Then switch on m_churn_vec_sel_ to pick the right debug instance.
    //
    //  const std::vector<uint32_t>* churn = nullptr;
    //  switch (m_churn_vec_sel_) {
    //      case 0: churn = &m_dbg_cells_.slot_churn();   break;
    //      case 1: churn = &m_dbg_food_.slot_churn();    break;
    //      case 2: churn = &m_dbg_bodies_.slot_churn();  break;
    //      case 3: churn = &m_dbg_springs_.slot_churn(); break;
    //  }

    // ── Aggregate churn stats ─────────────────────────────────────────────────
    OVecDebugImGuiSnapshot s;
    switch (m_churn_vec_sel_)
    {
        case 0:  s = snap.render.cell_debug_snapshot; break;
        case 1:  s = snap.render.food_debug_snapshot; break;
        case 2:  s = snap.render.body_debug_snapshot; break;
        default: s = snap.render.spring_debug_snapshot; break;
    }

    ImGui::Columns(3, "##churn_agg", false);
    ImGui::Text("Total reuse events:  %llu", s.total_churn);
    ImGui::NextColumn();
    ImGui::Text("Avg per slot:  %.2f", s.avg_churn);
    ImGui::NextColumn();
    ImGui::Text("Array size:  %d", s.array_size);
    ImGui::Columns(1);

    // ── Slot heatmap ──────────────────────────────────────────────────────────
    //  Each slot is drawn as a tiny colour-coded cell.
    //  Colour ramps from cold (dark blue) at 0 reuses to hot (red) at max.
    //  Hovering shows the exact slot index and churn count.
    //
    section("Slot Heatmap  (hover for detail)");

    // IMGUI TODO: replace stub array with real slot_churn_ data (see above).
    // The code below is a fully wired placeholder that shows the layout logic.

    const float avail_w = ImGui::GetContentRegionAvail().x;
    const int   n_slots = std::max(1, s.array_size);
    const float cell_w = std::max(k_heatmap_min_w,
        avail_w / static_cast<float>(n_slots));
    const int   per_row = std::max(1, static_cast<int>(avail_w / cell_w));

    const ImVec2 cursor_start = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImGui::Dummy({ avail_w, k_mini_bar_height * std::ceil(n_slots / (float)per_row) });

    for (int slot = 0; slot < n_slots; ++slot)
    {
        // IMGUI TODO: replace this stub value with (*churn)[slot]
        const uint32_t reuses = 0u; // stub — wire to real data

        // Normalise against the peak churn for the colour ramp
        // IMGUI TODO: compute max_reuse = *std::max_element(churn->begin(), churn->end())
        const uint32_t max_reuse = 1u; // stub
        const float t = (max_reuse > 0) ? static_cast<float>(reuses) / max_reuse : 0.f;

        // Heatmap colour: blue(cold) → cyan → yellow → red(hot)
        ImVec4 col;
        if (t < 0.25f) col = { 0.0f,       4.f * t,         1.0f,          1.f };
        else if (t < 0.50f) col = { 0.0f,        1.0f,           1.f - (t - 0.25f) * 4, 1.f };
        else if (t < 0.75f) col = { (t - 0.50f) * 4, 1.0f,           0.0f,          1.f };
        else                col = { 1.0f,         1.f - (t - 0.75f) * 4, 0.0f,         1.f };

        const int   row = slot / per_row;
        const int   col_idx = slot % per_row;
        const float x0 = cursor_start.x + col_idx * cell_w;
        const float y0 = cursor_start.y + row * (k_mini_bar_height + 1.f);
        const float x1 = x0 + cell_w - 1.f;
        const float y1 = y0 + k_mini_bar_height;

        draw_list->AddRectFilled({ x0, y0 }, { x1, y1 },
            ImGui::ColorConvertFloat4ToU32(col));

        // Hover tooltip
        if (ImGui::IsMouseHoveringRect({ x0, y0 }, { x1, y1 }))
        {
            ImGui::BeginTooltip();
            ImGui::Text("Slot #%d  —  %u reuse(s)", slot, reuses);
            ImGui::EndTooltip();
        }
    }

    // ── Top-N most churned slots table ────────────────────────────────────────
    section("Most Churned Slots");

    // IMGUI TODO: build sorted top-N list from slot_churn_ and render table.
    //
    // Skeleton:
    //   std::vector<std::pair<uint32_t,uint32_t>> entries;
    //   entries.reserve(churn->size());
    //   for (uint32_t j = 0; j < churn->size(); ++j)
    //       if ((*churn)[j] > 0) entries.push_back({ j, (*churn)[j] });
    //   std::sort(entries.begin(), entries.end(),
    //       [](auto& a, auto& b){ return a.second > b.second; });
    //   const int shown = std::min(m_churn_top_n_, (int)entries.size());

    if (ImGui::BeginTable("##churn_top", 3,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_WidthFixed, 60.f);
        ImGui::TableSetupColumn("Reuses", ImGuiTableColumnFlags_WidthFixed, 70.f);
        ImGui::TableSetupColumn("Bar", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        // IMGUI TODO: replace loop body with real entries[j] data.
        // Shown here as a stub with 0 rows to prevent crashes until wired.
        const int stub_shown = 0;
        for (int j = 0; j < stub_shown; ++j)
        {
            const uint32_t slot_idx = 0; // entries[j].first
            const uint32_t count = 0; // entries[j].second
            const uint32_t max_c = 1; // entries[0].second

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("#%u", slot_idx);
            ImGui::TableSetColumnIndex(1); ImGui::Text("%u", count);
            ImGui::TableSetColumnIndex(2);

            const float frac = (max_c > 0) ? static_cast<float>(count) / max_c : 0.f;
            char lbl[16];
            snprintf(lbl, sizeof(lbl), "x%u", count);
            colored_bar(frac, k_vec_colours[m_churn_vec_sel_], lbl,
                { -1.f, k_mini_bar_height });
        }
        ImGui::EndTable();
    }

    if (s.total_churn == 0)
        ImGui::TextDisabled("No churn recorded yet — use dbg.add() / dbg.emplace() to populate.");
}


// ═════════════════════════════════════════════════════════════════════════════
//  SUB-TAB 4 — Snapshots
//  Take, label, list, and diff named snapshots of any vector.
// ═════════════════════════════════════════════════════════════════════════════

void OVecDebugTab::draw_snapshots_tab(const SimSnapshot& snap, ImGuiContext& ctx)
{
    // ── Vector selector ───────────────────────────────────────────────────────
    ImGui::TextDisabled("Vector:");
    ImGui::SameLine();
    for (int i = 0; i < 4; ++i)
    {
        if (i > 0) ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, k_vec_colours[i]);
        if (ImGui::RadioButton(k_vec_names[i], m_snap_vec_sel_ == i))
        {
            m_snap_vec_sel_ = i;
            m_snap_a_idx_ = 0;
            m_snap_b_idx_ = 0;
        }
        ImGui::PopStyleColor();
    }

    ImGui::Separator();
    ImGui::Spacing();

    // ── Take snapshot button + optional label ─────────────────────────────────
    static char snap_label_buf[64] = "";
    ImGui::SetNextItemWidth(200.f);
    ImGui::InputText("Label##snap_lbl", snap_label_buf, sizeof(snap_label_buf));
    ImGui::SameLine();

    if (ImGui::Button("Take Snapshot"))
    {
        // IMGUI TODO: dispatch to the correct debug instance via m_snap_vec_sel_.
        // Example:
        //   const std::string lbl = strlen(snap_label_buf) > 0
        //       ? snap_label_buf : "";
        //   switch (m_snap_vec_sel_) {
        //       case 0: m_dbg_cells_.take_snapshot(lbl);   break;
        //       case 1: m_dbg_food_.take_snapshot(lbl);    break;
        //       case 2: m_dbg_bodies_.take_snapshot(lbl);  break;
        //       case 3: m_dbg_springs_.take_snapshot(lbl); break;
        //   }
        //   snap_label_buf[0] = '\0'; // clear label
        ImGui::TextDisabled("(snapshot would be taken here)"); // stub
    }

    ImGui::Spacing();

    // ── Snapshot table for the selected vector ────────────────────────────────
    section("Snapshot History");

    // IMGUI TODO: retrieve snapshot list from the active debug instance.
    //  const int n_snaps = (m_snap_vec_sel_ == 0) ? m_dbg_cells_.snapshot_count() : ...
    //  Use dbg.get_snapshot(i) to read each Snapshot struct.

    const int n_snaps_stub = 0; // stub
    if (n_snaps_stub == 0)
    {
        ImGui::TextDisabled("No snapshots taken yet.");
    }
    else if (ImGui::BeginTable("##snap_table", 6,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit,
        { -1.f, 140.f }))
    {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 30.f);
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Active", ImGuiTableColumnFlags_WidthFixed, 60.f);
        ImGui::TableSetupColumn("Free", ImGuiTableColumnFlags_WidthFixed, 50.f);
        ImGui::TableSetupColumn("Fill", ImGuiTableColumnFlags_WidthFixed, 55.f);
        ImGui::TableSetupColumn("Frag", ImGuiTableColumnFlags_WidthFixed, 55.f);
        ImGui::TableHeadersRow();

        // IMGUI TODO: iterate real snapshots.
        // for (int i = 0; i < n_snaps; ++i) {
        //     const auto& sn = active_dbg.get_snapshot(i);
        //     ImGui::TableNextRow();
        //     ImGui::TableSetColumnIndex(0); ImGui::Text("%d",   i);
        //     ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(sn.label.c_str());
        //     ImGui::TableSetColumnIndex(2); ImGui::Text("%d",   sn.active_count);
        //     ImGui::TableSetColumnIndex(3); ImGui::Text("%d",   sn.free_count);
        //     ImGui::TableSetColumnIndex(4); ImGui::Text("%.2f", sn.fill_ratio);
        //     ImGui::TableSetColumnIndex(5); ImGui::Text("%.2f", sn.fragmentation);
        // }
        ImGui::EndTable();
    }

    // ── Diff panel ────────────────────────────────────────────────────────────
    section("Diff Two Snapshots");

    ImGui::SetNextItemWidth(120.f);
    ImGui::InputInt("From (index)##snap_a", &m_snap_a_idx_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.f);
    ImGui::InputInt("To (index)##snap_b", &m_snap_b_idx_);
    ImGui::SameLine();

    if (ImGui::Button("Diff##run_diff") && n_snaps_stub >= 2)
    {
        // IMGUI TODO: call active_dbg.diff_snapshots(m_snap_a_idx_, m_snap_b_idx_)
        // and display the SnapshotDiff struct fields below.
    }

    // IMGUI TODO: show the last computed SnapshotDiff in a table here.
    // Fields: delta_active, delta_free, delta_array_size, delta_fill_ratio,
    //         delta_fragmentation, delta_emplaces, delta_adds, delta_removes,
    //         delta_failed_adds, delta_churn, elapsed_ms.
    ImGui::TextDisabled("[Diff results will appear here after clicking Diff]");
}


// ═════════════════════════════════════════════════════════════════════════════
//  SUB-TAB 5 — Integrity
//  On-demand integrity checks with a scrollable pass/fail log.
// ═════════════════════════════════════════════════════════════════════════════

void OVecDebugTab::draw_integrity_tab(const SimSnapshot& snap, ImGuiContext& ctx)
{
    // ── Control row ───────────────────────────────────────────────────────────
    const bool run_all = ImGui::Button("Run All Checks");
    ImGui::SameLine();

    const bool run_cells = ImGui::Button("Cells##ic");   ImGui::SameLine();
    const bool run_food = ImGui::Button("Food##ic");    ImGui::SameLine();
    const bool run_bodies = ImGui::Button("Bodies##ic");  ImGui::SameLine();
    const bool run_springs = ImGui::Button("Springs##ic");

    ImGui::SameLine();
    if (ImGui::Button("Clear Log##ic"))
    {
        m_integrity_log_.clear();
        m_integrity_all_pass_ = true;
    }

    // ── Execute checks and capture output into m_integrity_log_ ───────────────
    //  integrity_check() writes to an ostream; we redirect it to a stringstream.
    auto run_check = [&](auto& dbg, const char* vec_name)
        {
            std::ostringstream oss;
            const bool ok = dbg.integrity_check(oss);
            m_integrity_all_pass_ = m_integrity_all_pass_ && ok;

            // Prefix each line with the vector name for clarity
            std::string line;
            std::istringstream iss(oss.str());
            while (std::getline(iss, line))
            {
                if (!line.empty())
                    m_integrity_log_.push_back(std::string("[") + vec_name + "]  " + line);
            }
        };

    if (run_all || run_cells)   run_check(m_dbg_cells_, "Cells");
    if (run_all || run_food)    run_check(m_dbg_food_, "Food");
    if (run_all || run_bodies)  run_check(m_dbg_bodies_, "Bodies");
    if (run_all || run_springs) run_check(m_dbg_springs_, "Springs");

    ImGui::Spacing();

    // ── Overall status banner ─────────────────────────────────────────────────
    if (m_integrity_log_.empty())
    {
        ImGui::TextDisabled("No checks run yet.  Press a button above.");
    }
    else if (m_integrity_all_pass_)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, k_col_ok);
        ImGui::Text("✓  All checks passed");
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Text, k_col_fail);
        ImGui::Text("✗  One or more integrity failures detected!");
        ImGui::PopStyleColor();
    }

    // ── Lifetime failure counters ─────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::TextDisabled("Lifetime integrity failures per vector:");
    ImGui::Columns(4, "##ic_fail_cols", false);

    const uint64_t failures[4] = {
        m_dbg_cells_.integrity_failures,
        m_dbg_food_.integrity_failures,
        m_dbg_bodies_.integrity_failures,
        m_dbg_springs_.integrity_failures,
    };

    for (int i = 0; i < 4; ++i)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, k_vec_colours[i]);
        if (failures[i] > 0)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, k_col_fail);
            ImGui::Text("%s  ✗ %llu", k_vec_names[i], failures[i]);
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::Text("%s  ✓ 0", k_vec_names[i]);
        }
        ImGui::PopStyleColor();
        if (i < 3) ImGui::NextColumn();
    }
    ImGui::Columns(1);

    // ── Scrollable log ────────────────────────────────────────────────────────
    section("Log");

    ImGui::BeginChild("##ic_log", { -1.f, -1.f }, true,
        ImGuiWindowFlags_HorizontalScrollbar);

    for (const std::string& entry : m_integrity_log_)
    {
        // Colour code: OK lines green, FAIL lines red, everything else default.
        const bool is_ok = entry.find("OK") != std::string::npos;
        const bool is_fail = entry.find("FAIL") != std::string::npos;

        if (is_ok)   ImGui::PushStyleColor(ImGuiCol_Text, k_col_ok);
        else if (is_fail) ImGui::PushStyleColor(ImGuiCol_Text, k_col_fail);

        ImGui::TextUnformatted(entry.c_str());

        if (is_ok || is_fail) ImGui::PopStyleColor();
    }

    // Auto-scroll to bottom when new log entries arrive
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
}