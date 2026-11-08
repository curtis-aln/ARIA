#include "o_vector_tab.h"


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
        snap.render.tendon_debug_snapshot
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
