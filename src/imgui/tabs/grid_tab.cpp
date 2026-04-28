#include "grid_tab.h"
#include <imgui.h>

void GridTab::draw(const SimSnapshot& snap, ImGuiContext& ctx)
{
    const float total = ImGui::GetContentRegionAvail().x;
    const float sp = ImGui::GetStyle().ItemSpacing.x;
    const float cw = (total - sp * 2.f) / 3.f;
    const float ch = -1.f;


    // ── Protozoa Grid ─────────────────────────────────────────────────────────
    ImGui::BeginChild("GR_cell", { cw, ch }, true);
    draw_grid_info("Protozoa Grid", snap.cell_grid, snap.toggles.track_spatial_grids);
    ImGui::EndChild();
    ImGui::SameLine();

    // ── Food Grid ─────────────────────────────────────────────────────────────
    ImGui::BeginChild("GR_food", { cw, ch }, true);
    draw_grid_info("Food Grid", snap.food_grid, snap.toggles.track_spatial_grids);
    ImGui::EndChild();
    ImGui::SameLine();

    // ── Tuning ────────────────────────────────────────────────────────────────
    ImGui::BeginChild("GR_tune", { -1.f, ch }, true);
    ImGui::TextDisabled("Tuning");
    ImGui::Separator(); 

    toggle(snap, ctx, "Track occupancy", &WorldToggles::track_spatial_grids);
     ImGui::SameLine(); ImGui::TextDisabled("(costs perf)");

    ImGui::Spacing();
    static int res = static_cast<int>(snap.cell_grid.cells_x);
    ImGui::SetNextItemWidth(-1.f);
    ImGui::SliderInt("##gridres", &res, 10, 500, "Resolution %d x %d");

    ImGui::Spacing();
    if (ImGui::Button("Apply Cell Grid ##grid", { -1.f, 0.f }))
    {
        SimCommand cmd{ CommandType::SetCellGridResolution };
        cmd.int_val = res;
        ctx.push(cmd);
    }
    if (ImGui::Button("Apply Food Grid ##grid", { -1.f, 0.f }))
    {
        SimCommand cmd{ CommandType::SetFoodGridResolution };
		cmd.int_val = res;
        ctx.push(cmd);
    }

    ImGui::EndChild();
}

void GridTab::draw_grid_info(const char* label, const SpatialGridData& grid, const bool tracking)
{
    ImGui::TextDisabled("%s", label);
    ImGui::Separator();

	int total_cells = grid.cells_x * grid.cells_y;
    ImGui::Text("%dx%d  (%d cells)", grid.cells_x, grid.cells_y, total_cells);
    ImGui::Text("Cell  %.1f x %.1f px", grid.cell_width, grid.cell_height);
    ImGui::Text("World %.0f x %.0f", grid.world_width, grid.world_height);
    ImGui::Text("Cap   %zu obj/cell", grid.cell_max_capacity);

    if (!tracking) { ImGui::Spacing(); ImGui::TextDisabled("(tracking disabled)"); return; }

    ImGui::Spacing();
    ImGui::Text("Objects  %d", grid.total);
    ImGui::Text("Avg/cell %.2f", total_cells > 0 ? static_cast<float>(grid.total) * grid.inv : 0.f);
    ImGui::Text("Max cell %d  (%.0f%%)", grid.max_in,
        grid.cell_max_capacity > 0 ? grid.max_in * 100.f / static_cast<float>(grid.cell_max_capacity) : 0.f);
    ImGui::Text("Full     %d  (%.1f%%)", grid.full, grid.full * 100.f * grid.inv);
    ImGui::Text("Empty    %d  (%.1f%%)", grid.empty, grid.empty * 100.f * grid.inv);

    if (grid.full > 0)
        ImGui::TextColored({ 1.f, 0.4f, 0.4f, 1.f },
            "[!] %d at cap — objects may drop", grid.full);
}