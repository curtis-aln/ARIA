#include "display_tab.h"
#include <imgui.h>
#include "../../context/sim_command.h"
#include "context/sim_snapshot.h"

void DisplayTab::draw(const SimSnapshot& snap, ImGuiContext& ctx)
{
    const float total = ImGui::GetContentRegionAvail().x;
    const float sp = ImGui::GetStyle().ItemSpacing.x;
    const float cw = (total - sp * 2.f) / 3.f;
    const float ch = -1.f;

    // ── Rendering ─────────────────────────────────────────────────────────────
    ImGui::BeginChild("DISP_rend", { cw, ch }, true);
    ImGui::TextDisabled("Rendering");
    ImGui::Separator();
    
    toggle(snap, ctx, "Enable Rendering", &WorldToggles::m_rendering_, "R");
    toggle(snap, ctx, "Simple Mode", &WorldToggles::simple_mode, "S");
    toggle(snap, ctx, "Debug Mode", &WorldToggles::debug_mode, "D");
    toggle(snap, ctx, "Cell Grid", &WorldToggles::draw_cell_grid, "G");
    toggle(snap, ctx, "Food Grid", &WorldToggles::draw_food_grid, "F");
    toggle(snap, ctx, "Track Statistics", &WorldToggles::track_statistics, "T");

    ImGui::EndChild();
    ImGui::SameLine();

    // ── Debug Overlays ────────────────────────────────────────────────────────
    ImGui::BeginChild("DISP_dbg", { cw, ch }, true);
    ImGui::TextDisabled("Debug Overlays  (debug mode only)");
    ImGui::Separator();

	toggle(snap, ctx, "Show Connections", &WorldToggles::show_connections, "C");
	toggle(snap, ctx, "Skeleton Mode", &WorldToggles::skeleton_mode, "K");
	toggle(snap, ctx, "Bounding Boxes", &WorldToggles::show_bounding_boxes, "B");
	toggle(snap, ctx, "Toggle Collisions", &WorldToggles::toggle_collisions, "C");

    ImGui::EndChild();
    ImGui::SameLine();

    // ── UI + Camera ───────────────────────────────────────────────────────────
    ImGui::BeginChild("DISP_ui", { -1.f, ch }, true);
    ImGui::TextDisabled("UI");
    ImGui::Separator();

	toggle(snap, ctx, "Hide Panels", &WorldToggles::hide_panels, "Q");

    ImGui::SetNextItemWidth(-1.f);
    if (ImGui::SliderFloat("##uiscale", &m_ui_scale_, 50.f, 200.f, "UI Scale %.0f%%"))
        ImGui::GetIO().FontGlobalScale = m_ui_scale_ / 100.f;

    ImGui::SetNextItemWidth(-1.f);
    ImGui::SliderFloat("##zoom", &m_zoom_slider_, 0.1f, 5.f, "Zoom %.2fx");
    ImGui::TextDisabled("TODO: wire to Camera::zoom()");

    ImGui::Spacing();
    ImGui::TextDisabled("Camera Lock");
    ImGui::Separator();

    const char* lock_opts[] = { "Free", "Lock to selected", "Lock to most successful" };
    ImGui::SetNextItemWidth(-1.f);
    ImGui::Combo("##camlock", &m_camera_lock_, lock_opts, 3);
    ImGui::TextDisabled("TODO: wire to camera_follow logic");

    ImGui::EndChild();
}
