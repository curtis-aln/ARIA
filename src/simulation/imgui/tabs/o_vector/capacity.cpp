#include "o_vector_tab.h"


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
        snap.render.tendon_debug_snapshot
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