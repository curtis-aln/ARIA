#include "o_vector_tab.h"


// ═════════════════════════════════════════════════════════════════════════════
//  SUB-TAB 3 — Churn
//  Per-slot reuse heatmap and a sortable top-N table of hottest slots.
// ═════════════════════════════════════════════════════════════════════════════

static constexpr float k_mini_bar_height = 8.f;    // churn heatmap row height

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
    default: s = snap.render.tendon_debug_snapshot; break;
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
