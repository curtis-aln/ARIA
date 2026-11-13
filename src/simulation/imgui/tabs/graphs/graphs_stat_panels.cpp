#include "graphs_tab.h"
#include "stat_row.h"
#include <imgui.h>

void GraphsTab::draw_stat_panels(const SimSnapshot& snap)
{
    const float avail_h = ImGui::GetContentRegionAvail().y;
    const float panel_h = avail_h;

    // ── Population ────────────────────────────────────────────────────────────
    ImGui::BeginChild("ST_pop", { -1.f, panel_h }, true);
    ImGui::TextDisabled("Population");
    ImGui::Separator();
    const int  p = snap.cell_manager_stats.cell_count;
    const int  f = snap.food_manager_stats.food_count;
	const int  m = snap.cell_manager_stats.cell_matter_count;
	const int  s = snap.cell_manager_stats.tendon_count;

    const bool risk = p <= 10;
    StatRow::draw("Cells", "%d", p);
    StatRow::draw("Food", "%d", f);
    StatRow::draw("Matter", "%d", m);
    StatRow::draw("Springs", "%d", s);
    StatRow::draw("Total", "%d", p + f + m + s);
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
    StatRow::draw("Avg springs", "%.2f", snap.cell_manager_stats.average_tendon_count);
    StatRow::draw("Avg offspring", "%.2f", snap.cell_manager_stats.average_offspring_count);
    StatRow::draw("Avg energy", "%.1f", snap.cell_manager_stats.average_energy);
    StatRow::draw("Energy ratio", "%.3f", snap.cell_manager_stats.energy_efficiency);
    ImGui::EndChild();
}