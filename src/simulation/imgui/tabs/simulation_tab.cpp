#include "simulation_tab.h"
#include "../helpers/plot_utils.h"
#include <imgui.h>

#include "world/world_settings.h"

void SimulationTab::draw(const SimSnapshot& snap, ImGuiContext& ctx)
{
    auto slider_float_cmd = [&](const char* label, float current, float min, float max,
        const char* fmt, CommandType type)
        {
            float val = current;
            if (ImGui::SliderFloat(label, &val, min, max, fmt))
            {
                SimCommand cmd;
                cmd.type = type;
                cmd.float_val = val;
                ctx.push(cmd);
            }
        };

    // ── 4 panels: Playback | Fast Forward | World Settings | Save/Load + Keybinds
    const float total = ImGui::GetContentRegionAvail().x;
    const float sp = ImGui::GetStyle().ItemSpacing.x;
    const float cw = (total - sp * 3.f) / 4.f;
    const float ch = -1.f;

    // ── Playback ──────────────────────────────────────────────────────────────
    ImGui::BeginChild("SIM_play", { cw, ch }, true);
    ImGui::TextDisabled("Playback");
    ImGui::Separator();

    ImGui::Text("Time:  %s", PlotUtils::format_time(snap.sim_stats.total_time_elapsed).c_str());
    ImGui::SameLine(150.0f); // fixed x-offset lines both columns up
    ImGui::Text("Rendering FPS %.1f", snap.sim_stats.rendering_frame_rate);

    ImGui::Text("Frame: %u", snap.world_stats.iterations_);
    ImGui::SameLine(150.0f);
    ImGui::Text("Updating FPS %.1f", snap.sim_stats.updating_frame_rate);

    const float bw = (ImGui::GetContentRegionAvail().x - sp) * 0.5f;
    if (ImGui::Button(snap.toggles.paused ? "Resume [Spc]" : "Pause  [Spc]", { bw, 0.f }))
    {
		SimCommand cmd{ .section = CommandSection::WorldEvent, .type = CommandType::SetWorldToggles, .toggles = ctx.toggles };
		ctx.toggles.paused = !snap.toggles.paused;
		ctx.push(cmd);
    }

    ImGui::SameLine();
    if (ImGui::Button("Step [O]", { -1.f, 0.f }))
    {
		SimCommand cmd{ .section = CommandSection::WorldEvent, .type = CommandType::SetWorldToggles, .toggles = ctx.toggles };
		ctx.toggles.m_tick_frame_time = true;
		ctx.toggles.paused = true; // stepping implies pausing
		ctx.push(cmd);
    }

    ImGui::Spacing();
    ImGui::SetNextItemWidth(-1.f);

    {
        constexpr float kSliderMax = 210.f; // top 10 units = "Max" zone
        constexpr float kMaxThreshold = 200.f;

        const bool is_unlimited = snap.sim_stats.max_frame_rate_updating <= 0.f;
        float fps_val = is_unlimited ? kSliderMax : snap.sim_stats.max_frame_rate_updating;

        const char* fmt = is_unlimited ? "Updating Max FPS: MAX" : "Updating Max FPS %.1f";

        if (ImGui::SliderFloat("##updating fps", &fps_val, 30.f, kSliderMax, fmt))
        {
            SimCommand cmd{ .section = CommandSection::WorldEvent, .type = CommandType::SetUpdatingFrameRate, .float_val = (fps_val > kMaxThreshold) ? 0.f : fps_val };
            ctx.push(cmd);
        }
    }

    ImGui::Spacing();
    ImGui::SetNextItemWidth(-1.f);
    {
        constexpr float kSliderMax = 150.f; // top 10 units = "Max" zone
        constexpr float kMaxThreshold = 140.f;

        const bool is_unlimited = snap.sim_stats.max_frame_rate_rendering <= 0.f;
        float fps_val = is_unlimited ? kSliderMax : snap.sim_stats.max_frame_rate_rendering;

        const char* fmt = is_unlimited ? "Rendering Max FPS: MAX" : "Rendering Max FPS %.1f";

        if (ImGui::SliderFloat("##rendering fps", &fps_val, 30.f, kSliderMax, fmt))
        {
            SimCommand cmd{ .section = CommandSection::WorldEvent, .type = CommandType::SetRenderingFrameRate, .float_val = (fps_val > kMaxThreshold) ? 0.f : fps_val };
            ctx.push(cmd);
        }
    }

    ImGui::Spacing();
    if (ImGui::Button("Reset Simulation", { -1.f, 0.f }))
    {
		SimCommand cmd{ .section = CommandSection::WorldEvent, .type = CommandType::ResetSimulation };
		ctx.push(cmd);
    }

    ImGui::TextDisabled("UI & Camera");
    ImGui::Separator();

    toggle(snap, ctx, "Hide Panels", &WorldToggles::hide_panels, "Q");

    ImGui::SetNextItemWidth(-1.f);
    if (ImGui::SliderFloat("##uiscale", &m_ui_scale_, 50.f, 200.f, "UI Scale %.0f%%"))
        ImGui::GetIO().FontGlobalScale = m_ui_scale_ / 100.f;

    ImGui::SetNextItemWidth(-1.f);
    m_zoom_slider_ = snap.sim_stats.camera_zoom; // sync with sim state
    if (ImGui::SliderFloat("##zoom", &m_zoom_slider_, 0.0025f, 11.f, "Zoom %.3fx", ImGuiSliderFlags_Logarithmic))
    {
        SimCommand cmd{
            .section = CommandSection::WorldEvent,
            .type = CommandType::SetZoomLevel,
            .float_val = m_zoom_slider_ };
        ctx.push(cmd);
    }
    ImGui::Spacing();

    ImGui::SetNextItemWidth(-1.f);
    ImGui::SliderFloat("##camfriction", &m_camera_friction_, 0.f, 1.f, "Camera Friction %.2f"); // controling the friction of the camera movement (depricated)

    ImGui::EndChild();
    ImGui::SameLine();

    // ── Mouse Tools ───────────────────────────────────────────────────────────────
    ImGui::BeginChild("SIM_ff", { cw, ch }, true);
    ImGui::TextDisabled("Mouse Tools");
    ImGui::Separator();

    // ── Mode toggle (Add / Remove / Attract / Repel) ──────────────────────────────
    const float hw = (ImGui::GetContentRegionAvail().x - sp) * 0.5f;

    auto mode_button = [&](const char* label, int mode, ImVec4 active_color, bool same_line, bool last_in_row)
        {
            if (same_line) ImGui::SameLine();

            bool is_active = (m_mouse_mode_ == mode);
            if (is_active) ImGui::PushStyleColor(ImGuiCol_Button, active_color);

            if (ImGui::Button(label, { last_in_row ? -1.f : hw, 0.f }))
            {
                m_mouse_mode_ = mode;
                SimCommand cmd{ .section = CommandSection::WorldEvent, .type = CommandType::SetMouseMode, .int_val = mode };
                ctx.push(cmd);
            }

            if (is_active) ImGui::PopStyleColor();
        };

    // Row 1: Add / Remove
    mode_button("Add##mode", 0, { 0.2f, 0.5f, 0.2f, 1.f }, false, false);
    mode_button("Remove##mode", 1, { 0.6f, 0.2f, 0.2f, 1.f }, true, true);

    // Row 2: Attract / Repel
    mode_button("Attract##mode", 2, { 0.2f, 0.35f, 0.6f, 1.f }, false, false);
    mode_button("Repel##mode", 3, { 0.55f, 0.35f, 0.15f, 1.f }, true, true);

    ImGui::Spacing();

    // ── Checkboxes (shared state across all modes) ────────────────────────────────
    const char* mode_label =
        m_mouse_mode_ == 0 ? "Add:" :
        m_mouse_mode_ == 1 ? "Remove:" :
        m_mouse_mode_ == 2 ? "Attract:" : "Repel:";
    ImGui::TextDisabled(mode_label);

    if (ImGui::Checkbox("Cells##shared", &m_cells_))
    {
        SimCommand cmd{ .section = CommandSection::WorldEvent, .type = CommandType::SetWorldToggles, .toggles = ctx.toggles };
        ctx.toggles.mouse_add_cells = m_cells_;
        ctx.toggles.mouse_rem_cells = m_cells_;
        ctx.push(cmd);
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Food##shared", &m_food_))
    {
        SimCommand cmd{ .section = CommandSection::WorldEvent, .type = CommandType::SetWorldToggles, .toggles = ctx.toggles };
        ctx.toggles.mouse_add_food = m_food_;
        ctx.toggles.mouse_rem_food = m_food_;
        ctx.push(cmd);
    }

    ImGui::Spacing();
    ImGui::Separator();

    // ── Intensity and radius sliders ──────────────────────────────────────────────
    ImGui::SetNextItemWidth(-1.f);
	m_mouse_intensity_ = snap.world_stats.mouse_intensity; // sync with sim state
    if (ImGui::SliderInt("##intensity", &m_mouse_intensity_, 1, 25, "Intensity %d%"))
    {
        SimCommand cmd{ .section = CommandSection::WorldEvent, .type = CommandType::SetMouseIntensity, .int_val = m_mouse_intensity_ };
        ctx.push(cmd);
    }

    ImGui::SetNextItemWidth(-1.f);
	m_mouse_radius_ = snap.world_stats.mouse_radius; // sync with sim state
    if (ImGui::SliderFloat("##radius", &m_mouse_radius_, 200.f, 10000.f, "Radius %.0f"))
    {
        SimCommand cmd{ .section = CommandSection::WorldEvent, .type = CommandType::SetInfluenceRadius, .float_val = m_mouse_radius_ };
        ctx.push(cmd);
    }

    ImGui::Spacing();

	m_show_influence_radius_ = snap.toggles.show_influence_radius; // sync with sim state
    if (ImGui::Checkbox("Show Influence Radius##other", &m_show_influence_radius_))
    {
        SimCommand cmd{ .section = CommandSection::WorldEvent, .type = CommandType::SetWorldToggles, .toggles = ctx.toggles };
        ctx.toggles.show_influence_radius = m_show_influence_radius_;
        ctx.push(cmd);
    }

    ImGui::Spacing();

    ImGui::Text("Highlighted Cells: %u", snap.world_stats.highlighted_cells);
    ImGui::Text("Highlighted Food: %u", snap.world_stats.highlighted_food);

    ImGui::EndChild();
    ImGui::SameLine();


    // ── World Settings ────────────────────────────────────────────────────────
    ImGui::BeginChild("Nautral Selection", { cw, ch }, true);
    ImGui::TextDisabled("Springs");
    ImGui::Separator();

    ImGui::SetNextItemWidth(-1.f);
	m_spring_breaking_force_ = snap.cell_manager_stats.spring_breaking_force;
    if (ImGui::SliderFloat("##breaking force", &m_spring_breaking_force_, 0.f, 30.f, "breaking force %.2f"))
    {
        SimCommand cmd{ .section = CommandSection::WorldEvent, .type = CommandType::SetSpringBreakingForce, .float_val = m_spring_breaking_force_ };
        ctx.push(cmd);
    }

    ImGui::SetNextItemWidth(-1.f);
	m_spring_breaking_length_ = snap.cell_manager_stats.spring_breaking_length;
    if (ImGui::SliderFloat("##breaking Length", &m_spring_breaking_length_, 0.f, 400.f, "breaking Length %.2f"))
    {
        SimCommand cmd{ .section = CommandSection::WorldEvent, .type = CommandType::SetSpringBreakingLength, .float_val = m_spring_breaking_length_ };
        ctx.push(cmd);
    }

    ImGui::SetNextItemWidth(-1.f);
	m_spring_damage_threshold_ = snap.cell_manager_stats.spring_damage_threshold;
    if (ImGui::SliderFloat("##Damage Threshold", &m_spring_damage_threshold_, 0.f, 1.f, "Damage Threshold %.2f"))
    {
        SimCommand cmd{ .section = CommandSection::WorldEvent, .type = CommandType::SetSpringDamageThreshold, .float_val = m_spring_damage_threshold_ };
        ctx.push(cmd);
    }

    ImGui::SetNextItemWidth(-1.f);
	m_spring_work_const_ = snap.cell_manager_stats.spring_work_const;
    if (ImGui::SliderFloat("##Spring Work Const", &m_spring_work_const_, 0.f, 0.001f, "Spring Work Const %.6f"))
    {
        SimCommand cmd{ .section = CommandSection::WorldEvent, .type = CommandType::SetSpringWorkConst, .float_val = m_spring_work_const_ };
        ctx.push(cmd);
    }

	ImGui::Separator();
    ImGui::TextDisabled("Toggles");
    toggle(snap, ctx, "Toggle Collisions", &WorldToggles::toggle_collisions, "C");
    toggle(snap, ctx, "Enable Rendering", &WorldToggles::m_rendering_, "R");
    toggle(snap, ctx, "Simple Mode", &WorldToggles::simple_mode, "S");
    toggle(snap, ctx, "Debug Mode", &WorldToggles::debug_mode, "D");
    toggle(snap, ctx, "Cell Grid", &WorldToggles::draw_cell_grid, "G");
    toggle(snap, ctx, "Food Grid", &WorldToggles::draw_food_grid, "F");
    toggle(snap, ctx, "Track Statistics", &WorldToggles::track_statistics, "T");

    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("SIM_io", { -1.f, ch }, true);
    // ── Save / Load + Keybinds ────────────────────────────────────────────────
    /*
    
    ImGui::TextDisabled("Save / Load");
    ImGui::Separator();

    if (ImGui::Button("Save World", { -1.f, 0.f })) {  }
    if (ImGui::Button("Load World", { -1.f, 0.f })) {  }
    if (ImGui::Button("Save Settings JSON", { -1.f, 0.f })) {  }
    if (ImGui::Button("Load Settings JSON", { -1.f, 0.f })) {  }
    */

    ImGui::Spacing();
    ImGui::TextDisabled("Keybinds");
    ImGui::Separator();

    // Compact two-column keybind list — no table header to save vertical space
    if (ImGui::BeginTable("##keys", 2,
        ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
    {
        auto row = [](const char* a, const char* k)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(a);
                ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("%s", k);
            };

        row("Pause / Resume", "Space");  row("Step frame", "O");
        row("Toggle render", "R");      row("Hide panels", "Q");
        row("Cell grid", "G");      row("Food grid", "F");
        row("Collisions", "C");      row("Simple mode", "S");
        row("Debug mode", "D");      row("Track stats", "T");
        row("Skeleton mode", "K");      row("Bounding boxes", "B");
        row("Zoom", "Scroll"); row("Pan", "LMB drag");
        row("Select organism", "LMB");    row("Exit", "Esc");

        ImGui::EndTable();
    }

    ImGui::EndChild();
}
