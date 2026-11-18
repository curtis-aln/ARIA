#include "../graphs/plot_utils.h"
#include "simulation_tab.h"
#include <cstdio>
#include <imgui.h>

#include <simulation/context/sim_command.h>
#include <simulation/context/sim_snapshot.h>
#include <simulation/context/state.h>
#include "../../../../Utils/Graphics/post_fx.h"   // adjust the relative path
#include <world/world_settings.h>
#include <simulation/settings/settings.h>

namespace
{
	


	// Frame-rate cap slider: the far right end of the slider means "unlimited" (sent as 0)
	void fps_cap_slider(ImGuiContext& ctx, const char* id, const char* name, const float current_cap,
		const float slider_max, const float unlimited_above, const CommandType type, const char* tooltip)
	{
		const bool unlimited = current_cap <= 0.f;
		float v = unlimited ? slider_max : current_cap;

		char fmt[48];
		std::snprintf(fmt, sizeof fmt, unlimited ? "%s Max FPS: MAX" : "%s Max FPS %%.1f", name);

		ImGui::SetNextItemWidth(-1.f);
		if (ImGui::SliderFloat(id, &v, 30.f, slider_max, fmt))
		{
			SimCommand cmd{ .section = CommandSection::SimulationEvent, .type = type,
							.float_val = (v > unlimited_above) ? 0.f : v };
			ctx.push(cmd);
		}
		ITab::tip(tooltip);
	}
}

void SimulationTab::draw(const SimSnapshot& snap, ImGuiContext& ctx)
{
	// 4 panels: Playback | Mouse Tools | Springs + Toggles | UI & Camera
	const float total = ImGui::GetContentRegionAvail().x;
	const float sp = ImGui::GetStyle().ItemSpacing.x;
	const float cw = (total - sp * 3.f) / 4.f;
	const float ch = -1.f;

	draw_playback_panel(snap, ctx, cw, ch);  ImGui::SameLine();
	draw_mouse_panel(snap, ctx, cw, ch);     ImGui::SameLine();
	draw_toggles(snap, ctx, cw, ch);   ImGui::SameLine();
	draw_view_panel(snap, ctx, -1.f, ch);
}

// ── Playback ──────────────────────────────────────────────────────────────────
void SimulationTab::draw_playback_panel(const SimSnapshot& snap, ImGuiContext& ctx, const float w, const float h)
{
	ImGui::BeginChild("SIM_play", { w, h }, true);
	ImGui::TextDisabled("Playback");
	ImGui::Separator();

	if (ImGui::BeginTable("##play_stats", 2, ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableNextColumn(); ImGui::Text("Time:  %s", PlotUtils::format_time(snap.sim_stats.total_time_elapsed).c_str());
		ImGui::TableNextColumn(); ImGui::Text("Render FPS %.1f", snap.sim_stats.rendering_frame_rate);

		ImGui::TableNextColumn(); ImGui::Text("Frame: %u", snap.world_stats.iterations_);
		ImGui::TableNextColumn(); ImGui::Text("Update FPS %.1f", snap.sim_stats.updating_frame_rate);

		ImGui::TableNextColumn(); ImGui::Text("Iter/s: %.1f", snap.world_stats.iterations_per_second);
		ImGui::EndTable();
	}

	const float sp = ImGui::GetStyle().ItemSpacing.x;
	const float bw = (ImGui::GetContentRegionAvail().x - sp) * 0.5f;

	if (ImGui::Button(snap.world_toggles.paused ? "Resume [Spc]" : "Pause  [Spc]", { bw, 0.f }))
	{
		ctx.world_toggles.paused = !snap.world_toggles.paused;   // change first, then copy into the command
		SimCommand cmd{ .section = CommandSection::WorldEvent, .type = CommandType::SetWorldToggles, .world_toggles = ctx.world_toggles };
		ctx.push(cmd);
	}
	tip("Freeze or resume the simulation. The UI and camera keep running while paused.");

	ImGui::SameLine();
	if (ImGui::Button("Step [O]", { -1.f, 0.f }))
	{
		ctx.world_toggles.m_tick_frame_time = true;
		ctx.world_toggles.paused = true;   // stepping implies pausing
		SimCommand cmd{ .section = CommandSection::WorldEvent, .type = CommandType::SetWorldToggles, .world_toggles = ctx.world_toggles };
		ctx.push(cmd);
	}
	tip("Advance the simulation by one update, then stay paused.");

	ImGui::Spacing();
	fps_cap_slider(ctx, "##updating fps", "Updating", snap.sim_stats.max_frame_rate_updating,
		410.f, 400.f, CommandType::SetUpdatingFrameRate,
		"Upper limit on simulation updates per second. Drag fully right for no limit.");

	ImGui::Spacing();
	fps_cap_slider(ctx, "##rendering fps", "Rendering", snap.sim_stats.max_frame_rate_rendering,
		150.f, 140.f, CommandType::SetRenderingFrameRate,
		"Upper limit on drawn frames per second. Drag fully right for no limit.");

	ImGui::Spacing();
	ImGui::SetNextItemWidth(-1.f);
	int sub_iterations = WorldSettings::tick_sim_multiplier;
	if (ImGui::SliderInt("##sub_iterations", &sub_iterations, 1, 30, "update sub-iterations %d"))
		ctx.push({ .section = CommandSection::WorldEvent, .type = CommandType::SetSubIterations, .int_val = sub_iterations });
	tip("How many simulation sub-iterations run for each update. Higher values mean more work per update.");

	ImGui::Spacing();
	if (ImGui::Button("Reset Simulation", { -1.f, 0.f }))
	{
		warnings.show("Reset world", "Every organism will be deleted.\nThis can't be undone.",
			[&] { ctx.push({ .section = CommandSection::WorldEvent, .type = CommandType::ResetSimulation }); },
			{},                                   // optional on-cancel
			WarningPopup::Severity::Danger);
	}
	tip("Throw away the current world and start a fresh simulation.");

	warnings.draw();

	ImGui::EndChild();
}

// ── Mouse tools ───────────────────────────────────────────────────────────────
void SimulationTab::draw_mouse_panel(const SimSnapshot& snap, ImGuiContext& ctx, const float w, const float h)
{
	ImGui::BeginChild("SIM_ff", { w, h }, true);
	ImGui::TextDisabled("Mouse Tools");
	ImGui::Separator();

	draw_mouse_mode_buttons(ctx);

	ImGui::Spacing();
	if (m_mouse_mode_ != -1)
		draw_mouse_options(snap, ctx);
	else
		ImGui::TextDisabled("Pick a tool to see its options.");

	ImGui::EndChild();
}

void SimulationTab::draw_mouse_mode_buttons(ImGuiContext& ctx)
{
	const float sp = ImGui::GetStyle().ItemSpacing.x;
	const float hw = (ImGui::GetContentRegionAvail().x - sp) * 0.5f;

	auto mode_button = [&](const char* label, const int mode, const ImVec4 active_color,
		const bool same_line, const bool last_in_row, const char* tooltip)
		{
			if (same_line) ImGui::SameLine();

			const bool is_active = (m_mouse_mode_ == mode);
			if (is_active) ImGui::PushStyleColor(ImGuiCol_Button, active_color);

			if (ImGui::Button(label, { last_in_row ? -1.f : hw, 0.f }))
			{
				m_mouse_mode_ = is_active ? -1 : mode;   // clicking the active tool turns it off

				SimCommand cmd{ .section = CommandSection::SimulationEvent, .type = CommandType::SetMouseMode, .int_val = m_mouse_mode_ };
				ctx.push(cmd);

				ctx.world_toggles.show_influence_radius = (m_mouse_mode_ != -1);
				SimCommand toggle_cmd{ .section = CommandSection::WorldEvent, .type = CommandType::SetWorldToggles, .world_toggles = ctx.world_toggles };
				ctx.push(toggle_cmd);
			}
			tip(tooltip);

			if (is_active) ImGui::PopStyleColor();
		};

	mode_button("Add##mode", 0, { 0.2f, 0.5f, 0.2f, 1.f }, false, false,
		"Spawn the selected types inside the circle around the cursor. Click again to turn the tool off.");
	mode_button("Remove##mode", 1, { 0.6f, 0.2f, 0.2f, 1.f }, true, true,
		"Delete the selected types inside the circle around the cursor. Click again to turn the tool off.");

	mode_button("Attract##mode", 2, { 0.2f, 0.35f, 0.6f, 1.f }, false, false,
		"Pull the selected types toward the cursor. Click again to turn the tool off.");
	mode_button("Repel##mode", 3, { 0.55f, 0.35f, 0.15f, 1.f }, true, true,
		"Push the selected types away from the cursor. Click again to turn the tool off.");
}

// Only called while a tool is active
void SimulationTab::draw_mouse_options(const SimSnapshot& snap, ImGuiContext& ctx)
{
	const char* mode_label =
		m_mouse_mode_ == 0 ? "Add:" :
		m_mouse_mode_ == 1 ? "Remove:" :
		m_mouse_mode_ == 2 ? "Attract:" : "Repel:";
	ImGui::TextDisabled("%s", mode_label);

	if (ImGui::Checkbox("Cells##shared", &m_cells_))
	{
		ctx.world_toggles.mouse_add_cells = m_cells_;
		ctx.world_toggles.mouse_rem_cells = m_cells_;
		SimCommand cmd{ .section = CommandSection::WorldEvent, .type = CommandType::SetWorldToggles, .world_toggles = ctx.world_toggles };
		ctx.push(cmd);
	}
	tip("Apply the active tool to cells.");

	ImGui::SameLine();
	if (ImGui::Checkbox("Food##shared", &m_food_))
	{
		ctx.world_toggles.mouse_add_food = m_food_;
		ctx.world_toggles.mouse_rem_food = m_food_;
		SimCommand cmd{ .section = CommandSection::WorldEvent, .type = CommandType::SetWorldToggles, .world_toggles = ctx.world_toggles };
		ctx.push(cmd);
	}
	tip("Apply the active tool to food.");

	ImGui::Spacing();
	ImGui::Separator();

	ImGui::SetNextItemWidth(-1.f);
	m_mouse_intensity_ = snap.world_stats.mouse_intensity;   // sync with sim state
	if (ImGui::SliderInt("##intensity", &m_mouse_intensity_, 1, 25, "Intensity %d%%"))
	{
		SimCommand cmd{ .section = CommandSection::WorldEvent, .type = CommandType::SetMouseIntensity, .int_val = m_mouse_intensity_ };
		ctx.push(cmd);
	}
	tip("How strongly the tool acts each tick. Higher values affect more, or push harder.");

	synced_slider(ctx, "##radius", m_mouse_radius_, snap.world_stats.mouse_radius,
		200.f, 10000.f, "Radius %.0f", CommandSection::WorldEvent, CommandType::SetInfluenceRadius,
		"Size of the circle around the cursor that the tool affects, in world units.");

	ImGui::Spacing();
	ImGui::Text("Highlighted Cells: %u", snap.world_stats.highlighted_cells);
	ImGui::Text("Highlighted Food: %u", snap.world_stats.highlighted_food);
}

void SimulationTab::draw_background_options(const SimSnapshot& snap, ImGuiContext& ctx)
{
	const auto& presets = SimulationSettings::bg_presets;
	static int selected_preset = SimulationSettings::bg_preset_index;
	static float top_col[3] = {
		SimulationSettings::bg_colors[0].r / 255.f,
		SimulationSettings::bg_colors[0].g / 255.f,
		SimulationSettings::bg_colors[0].b / 255.f };
	static float bot_col[3] = {
		SimulationSettings::bg_colors[1].r / 255.f,
		SimulationSettings::bg_colors[1].g / 255.f,
		SimulationSettings::bg_colors[1].b / 255.f };

	if (ImGui::BeginCombo("##bg_preset", presets[selected_preset].name.c_str()))
	{
		for (int i = 0; i < static_cast<int>(presets.size()); ++i)
		{
			const bool is_selected = (selected_preset == i);
			if (ImGui::Selectable(presets[i].name.c_str(), is_selected))
			{
				selected_preset = i;
				if (presets[i].name != "Custom")
					ctx.push({ .section = CommandSection::SimulationEvent, .type = CommandType::SetBackgroundPreset, .int_val = i });
			}
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	if (presets[selected_preset].name == "Custom")
	{
		ImGui::Spacing();
		bool changed = false;
		changed |= ImGui::ColorEdit3("Top", top_col);
		changed |= ImGui::ColorEdit3("Bottom", bot_col);

		if (changed)
		{
			SimCommand cmd{ .section = CommandSection::SimulationEvent, .type = CommandType::SetCustomBackground };
			cmd.color_val_a = sf::Color(
				static_cast<uint8_t>(top_col[0] * 255.f),
				static_cast<uint8_t>(top_col[1] * 255.f),
				static_cast<uint8_t>(top_col[2] * 255.f));
			cmd.color_val_b = sf::Color(
				static_cast<uint8_t>(bot_col[0] * 255.f),
				static_cast<uint8_t>(bot_col[1] * 255.f),
				static_cast<uint8_t>(bot_col[2] * 255.f));
			ctx.push(cmd);
		}
	}
}


void SimulationTab::draw_toggles(const SimSnapshot& snap, ImGuiContext& ctx, const float w, const float h)
{
	ImGui::BeginChild("Toggles", { w, h }, true);
	ImGui::TextDisabled("Toggles");

	ImGui::Columns(2, nullptr, false);

	// ---- Column 1: simulation + grids ----
	ImGui::SeparatorText("Simulation");
	toggle(snap, ctx, "Collisions", &WorldToggles::toggle_collisions, "C");
	tip("Turn collisions between bodies on or off.");
	toggle(snap, ctx, "Rendering", &SimulationToggles::m_rendering_, "R");
	tip("Turn world drawing on or off. The simulation keeps running, which is useful for speed.");
	toggle(snap, ctx, "Track Stats", &WorldToggles::track_statistics, "T");
	tip("Record statistics over time for the graphs. Turn it off to save a little CPU.");

	ImGui::SeparatorText("Grids");
	toggle(snap, ctx, "Cell Grid", &WorldToggles::draw_collision_grid, "G");
	tip("Draw the collision grid that cells are sorted into.");
	toggle(snap, ctx, "Food Grid", &WorldToggles::draw_food_grid, "F");
	tip("Draw the grid that food is sorted into.");
	toggle(snap, ctx, "Collision Grid", &CellManagerToggles::show_newborn_grid);
	tip("Shows the collision grid behind the objects of the simulation.");
	toggle(snap, ctx, "Background Grid", &WorldToggles::draw_background_grid);
	tip("Shows a visual-only grid in the background of the simulation.");

	ImGui::NextColumn();

	// ---- Column 2: render filters + debug ----
	ImGui::SeparatorText("Render");
	toggle(snap, ctx, "Cells", &WorldToggles::render_cells);
	tip("Draw cells.");
	toggle(snap, ctx, "Food", &WorldToggles::render_food);
	tip("Draw food.");
	toggle(snap, ctx, "Cell Matter", &WorldToggles::render_cells_matter);
	tip("Draw cell matter.");
	toggle(snap, ctx, "World Border", &WorldToggles::draw_world_border);
	tip("Draw the world border. Useful for debugging world bounds.");

	ImGui::SeparatorText("Debug");
	toggle(snap, ctx, "Debug Mode", &WorldToggles::debug_mode, "D");
	tip("Show debug overlays and extra debug information.");
	toggle(snap, ctx, "Only Newborns", &CellManagerToggles::show_only_newborns);
	tip("Only show cells that have been recently born. Useful for debugging reproduction.");

	ImGui::Columns(1);

	ImGui::EndChild();
}

// ── UI & camera ───────────────────────────────────────────────────────────────
void SimulationTab::draw_view_panel(const SimSnapshot& snap, ImGuiContext& ctx, const float w, const float h)
{
	ImGui::BeginChild("World", { w, h }, true);
	ImGui::TextDisabled("UI & Camera");
	ImGui::Separator();

	ImGui::SetNextItemWidth(-1.f);
	if (ImGui::SliderFloat("##uiscale", &m_ui_scale_, 50.f, 200.f, "UI Scale %.0f%%"))
		ImGui::GetIO().FontGlobalScale = m_ui_scale_ / 100.f;
	tip("Scale the UI text, and with it the widgets. Does not change the simulation view.");

	synced_slider(ctx, "##zoom", m_zoom_slider_, snap.sim_stats.camera_zoom,
		0.0025f, 11.f, "Zoom %.3fx", CommandSection::SimulationEvent, CommandType::SetZoomLevel,
		"Camera zoom. The scale is logarithmic, so small zoom levels get more precision.",
		ImGuiSliderFlags_Logarithmic);

	ImGui::SetNextItemWidth(-1.f);
	ImGui::SliderFloat("##camfriction", &m_camera_friction_, 0.f, 1.f, "Camera Friction %.2f");
	tip("Damping of camera movement. Deprecated.");

	ImGui::Spacing();
	ImGui::TextDisabled("World Settings");
	ImGui::Separator();

	// slider for changing the world size
	ImGui::SetNextItemWidth(-1.f);
	if (ImGui::SliderFloat("##world_radius", &WorldSettings::bounds_radius, 1000, max_bounds_radius, "world radius %.0f"))
		ctx.push({ .section = CommandSection::WorldEvent, .type = CommandType::SetWorldRadius, .float_val = WorldSettings::bounds_radius });

	// toggle for central gravity
	toggle(snap, ctx, "central gravity", &WorldToggles::central_gravity);


	draw_background_options(snap, ctx);

	ImGui::EndChild();
}