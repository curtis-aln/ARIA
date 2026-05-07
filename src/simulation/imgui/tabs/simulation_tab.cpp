#include "simulation_tab.h"
#include "../helpers/plot_utils.h"
#include <imgui.h>

#include "world/world_settings.h"

void SimulationTab::draw(const SimSnapshot& snap, ImGuiContext& ctx)
{
    // ── 4 panels: Playback | Fast Forward | World Settings | Save/Load + Keybinds
    const float total = ImGui::GetContentRegionAvail().x;
    const float sp = ImGui::GetStyle().ItemSpacing.x;
    const float cw = (total - sp * 3.f) / 4.f;
    const float ch = -1.f;

    // ── Playback ──────────────────────────────────────────────────────────────
    ImGui::BeginChild("SIM_play", { cw, ch }, true);
    ImGui::TextDisabled("Playback");
    ImGui::Separator();

    ImGui::Text("Time:  %s", PlotUtils::format_time(snap.stats.m_total_time_elapsed_).c_str());
    ImGui::Text("Frame: %u", snap.stats.iterations_);
    ImGui::Spacing();

    const float bw = (ImGui::GetContentRegionAvail().x - sp) * 0.5f;
    if (ImGui::Button(snap.toggles.paused ? "Resume [Spc]" : "Pause  [Spc]", { bw, 0.f }))
    {
		SimCommand cmd{ CommandType::SetToggles };
		cmd.toggles.paused = !cmd.toggles.paused;
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
    ImGui::SliderFloat("##speed", &m_speed_, 0.1f, 10.f, "Speed %.1fx");

    ImGui::Spacing();
    if (ImGui::Button("Reset Simulation", { -1.f, 0.f }))
    {
		SimCommand cmd{ CommandType::ResetSimulation };
		ctx.push(cmd);
    }

    ImGui::Separator();
    ImGui::TextDisabled("Fast Forward");

    if (m_fast_fwd_) ImGui::PushStyleColor(ImGuiCol_Button, { 0.6f, 0.2f, 0.2f, 1.f });
    if (ImGui::Button(m_fast_fwd_ ? "Stop##ff" : "Start##ff", { -1.f, 0.f }))
        m_fast_fwd_ = !m_fast_fwd_;
    if (m_fast_fwd_) ImGui::PopStyleColor();

    ImGui::Spacing();
    const char* conds[] = { "Time elapsed (s)", "Population target", "Generation target" };
    int c = static_cast<int>(m_ff_cond_);
    ImGui::SetNextItemWidth(-1.f);
    if (ImGui::Combo("##ffcond", &c, conds, 3))
        m_ff_cond_ = static_cast<FFCondition>(c);
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputFloat("Target##ff", &m_ff_target_, 10.f, 100.f, "%.0f");
    ImGui::Spacing();

    ImGui::EndChild();
    ImGui::SameLine();

    // ── Fast Forward ──────────────────────────────────────────────────────────
    ImGui::BeginChild("SIM_ff", { cw, ch }, true);

    ImGui::EndChild();
    ImGui::SameLine();

    // ── World Settings ────────────────────────────────────────────────────────
    ImGui::BeginChild("SIM_world", { cw, ch }, true);
    ImGui::TextDisabled("World");
    ImGui::Separator();

    ImGui::SetNextItemWidth(-1.f);
    auto slider_float_cmd = [&](const char* label, float current, float min, float max,
        const char* fmt, CommandType type)
        {
            float val = current;
            if (ImGui::SliderFloat(label, &val, min, max, fmt))
            {
                SimCommand cmd;
                cmd.type = type;
                ctx.push(cmd);
            }
        };
    slider_float_cmd("##minsp", snap.toggles.min_speed, 0.f, 135.f, "Min Speed %.1f", CommandType::SetToggles);
	
    float ds = snap.toggles.delta_min_speed * 1000.f;
    ImGui::SetNextItemWidth(-1.f);

	slider_float_cmd("##dsp", ds, 0.2f, 2.f, "Delta Spd %.3f", CommandType::SetToggles);

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
