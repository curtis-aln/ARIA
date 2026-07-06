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

    ImGui::Text("Time:  %s", PlotUtils::format_time(snap.sim_state.total_time_elapsed).c_str());
    ImGui::Text("Frame: %u", snap.stats.iterations_);
    ImGui::Spacing();

    const float bw = (ImGui::GetContentRegionAvail().x - sp) * 0.5f;
    if (ImGui::Button(snap.toggles.paused ? "Resume [Spc]" : "Pause  [Spc]", { bw, 0.f }))
    {
		SimCommand cmd{ CommandType::SetToggles };
		cmd.toggles.paused = !snap.toggles.paused;
		ctx.push(cmd);
    }

    ImGui::SameLine();
    if (ImGui::Button("Step [O]", { -1.f, 0.f }))
    {
		SimCommand cmd{ CommandType::SetToggles };
		cmd.toggles.m_tick_frame_time = true;
		cmd.toggles.paused = true; // stepping implies pausing
		ctx.push(cmd);
    }

    ImGui::Spacing();
    ImGui::SetNextItemWidth(-1.f);

    {
        constexpr float kSliderMax = 210.f; // top 10 units = "Max" zone
        constexpr float kMaxThreshold = 200.f;

        const bool is_unlimited = snap.sim_state.max_frame_rate_updating <= 0.f;
        float fps_val = is_unlimited ? kSliderMax : snap.sim_state.max_frame_rate_updating;

        const char* fmt = is_unlimited ? "sim max fps: MAX" : "sim max fps %.1f";

        if (ImGui::SliderFloat("##fps", &fps_val, 30.f, kSliderMax, fmt))
        {
            SimCommand cmd{ CommandType::SetFrameRate };
            cmd.float_val = (fps_val > kMaxThreshold) ? 0.f : fps_val;
            ctx.push(cmd);
        }
    }

    ImGui::Spacing();
    if (ImGui::Button("Reset Simulation", { -1.f, 0.f }))
    {
		SimCommand cmd{ CommandType::ResetSimulation };
		ctx.push(cmd);
    }

    ImGui::Separator();
    ImGui::EndChild();
    ImGui::SameLine();

    // ── Mouse Tools ───────────────────────────────────────────────────────────────
    ImGui::BeginChild("SIM_ff", { cw, ch }, true);
    ImGui::TextDisabled("Mouse Tools");
    ImGui::Separator();

    // ── Add / Remove mode toggle ──────────────────────────────────────────────────
    const float hw = (ImGui::GetContentRegionAvail().x - sp) * 0.5f;

    if (m_mouse_mode_ == 0) ImGui::PushStyleColor(ImGuiCol_Button, { 0.2f, 0.5f, 0.2f, 1.f });
    if (ImGui::Button("Add##mode", { hw, 0.f }))
    {
        m_mouse_mode_ = 0;
        SimCommand cmd{ CommandType::SetToggles };
        cmd.toggles = snap.toggles;
        cmd.toggles.mouse_mode = 0;
        ctx.push(cmd);
    }
    if (m_mouse_mode_ == 0) ImGui::PopStyleColor();

    ImGui::SameLine();

    if (m_mouse_mode_ == 1) ImGui::PushStyleColor(ImGuiCol_Button, { 0.6f, 0.2f, 0.2f, 1.f });
    if (ImGui::Button("Remove##mode", { -1.f, 0.f }))
    {
        m_mouse_mode_ = 1;
        SimCommand cmd{ CommandType::SetToggles };
        cmd.toggles = snap.toggles;
        cmd.toggles.mouse_mode = 1;
        ctx.push(cmd);
    }
    if (m_mouse_mode_ == 1) ImGui::PopStyleColor();

    ImGui::Spacing();

    // ── Checkboxes for active mode ────────────────────────────────────────────────
    if (m_mouse_mode_ == 0)
    {
        ImGui::TextDisabled("Add:");
        if (ImGui::Checkbox("Cells##add", &m_add_cells_))
        {
            SimCommand cmd{ CommandType::SetToggles };
            cmd.toggles = snap.toggles;
            cmd.toggles.mouse_add_cells = m_add_cells_;
            ctx.push(cmd);
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Food##add", &m_add_food_))
        {
            SimCommand cmd{ CommandType::SetToggles };
            cmd.toggles = snap.toggles;
            cmd.toggles.mouse_add_food = m_add_food_;
            ctx.push(cmd);
        }
    }
    else
    {
        ImGui::TextDisabled("Remove:");
        if (ImGui::Checkbox("Cells##rem", &m_remove_cells_))
        {
            SimCommand cmd{ CommandType::SetToggles };
            cmd.toggles = snap.toggles;
            cmd.toggles.mouse_rem_cells = m_remove_cells_;
            ctx.push(cmd);
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Food##rem", &m_remove_food_))
        {
            SimCommand cmd{ CommandType::SetToggles };
            cmd.toggles = snap.toggles;
            cmd.toggles.mouse_rem_food = m_remove_food_;
            ctx.push(cmd);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // ── Intensity and radius sliders ──────────────────────────────────────────────
    ImGui::SetNextItemWidth(-1.f);
    if (ImGui::SliderFloat("##intensity", &m_mouse_intensity_, 0.1f, 10.f, "Intensity %.2f"))
    {
        SimCommand cmd{ CommandType::SetToggles };
        cmd.toggles = snap.toggles;
        cmd.toggles.mouse_intensity = m_mouse_intensity_;
        ctx.push(cmd);
    }

    ImGui::SetNextItemWidth(-1.f);
    if (ImGui::SliderFloat("##radius", &m_mouse_radius_, 10.f, 1000.f, "Radius %.0f"))
    {
        SimCommand cmd{ CommandType::SetToggles };
        cmd.toggles = snap.toggles;
        cmd.toggles.mouse_radius = m_mouse_radius_;
        ctx.push(cmd);
    }

    ImGui::EndChild();
    ImGui::SameLine();


    // ── World Settings ────────────────────────────────────────────────────────
    ImGui::BeginChild("SIM_world", { cw, ch }, true);
    ImGui::TextDisabled("World");
    ImGui::Separator();

    ImGui::SetNextItemWidth(-1.f);

    static float world_radius = WorldSettings::bounds_radius;
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputFloat("##wrad", &world_radius, 1000.f, 10000.f, "R=%.0f");
    ImGui::TextDisabled("TODO: wire World::resize()");

    ImGui::EndChild();
    ImGui::SameLine();

    // ── Save / Load + Keybinds ────────────────────────────────────────────────
    ImGui::BeginChild("SIM_io", { -1.f, ch }, true);
    ImGui::TextDisabled("Save / Load");
    ImGui::Separator();

    if (ImGui::Button("Save World", { -1.f, 0.f })) { /* TODO */ }
    if (ImGui::Button("Load World", { -1.f, 0.f })) { /* TODO */ }
    if (ImGui::Button("Save Settings JSON", { -1.f, 0.f })) { /* TODO */ }
    if (ImGui::Button("Load Settings JSON", { -1.f, 0.f })) { /* TODO */ }

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
