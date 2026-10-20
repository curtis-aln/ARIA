#include "../graphs/plot_utils.h"
#include "organism_tab.h"
#include <algorithm>
#include <cstdio>
#include <entities/body.h>
#include <entities/cell/cell.h>
#include <entities/spring/spring.h>
#include <imgui.h>
#include <managers/cell_manager/organism_tracker.h>
#include <simulation/context/sim_command.h>
#include <simulation/context/sim_snapshot.h>
#include <simulation/context/state.h>
#include <simulation/imgui/tabs/i_tab.h>
#include <vector>


// ─────────────────────────────────────────────────────────────────────────────
//  Top-level draw
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw(const SimSnapshot& snap, ImGuiContext& ctx)
{
	const OrganismTracker& protozoa = snap.protozoa_tracker;

	if (snap.protozoa_tracker.selected_id == -1)
	{
		draw_no_selection();
		return;
	}

	// The panel showing information about the protozoa as a whole
	ImGui::BeginChild("OV_panel", { 240.f, -1.f }, false);
	draw_overview(snap, ctx, protozoa);
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("TAB_panel", { -1.f, -1.f }, false);
	if (!ImGui::BeginTabBar("##org_tabs"))
	{
		ImGui::EndChild();
		return;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 6.f, 2.f });

	// The different tabs of this
	if (ImGui::BeginTabItem("Cells & Springs"))
	{
		draw_cells_springs_tab(snap, ctx, protozoa);
		ImGui::SameLine();
		draw_mutation_controls(snap, ctx, protozoa);
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Energy")) { draw_energy_tab(ctx, snap);           ImGui::EndTabItem(); }
	if (ImGui::BeginTabItem("Brain")) { Brain(); ImGui::EndTabItem(); }
	ImGui::PopStyleVar();

	ImGui::EndTabBar();
	ImGui::EndChild();
}

// ─────────────────────────────────────────────────────────────────────────────
//  No selection
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_no_selection()
{
	ImGui::Spacing();
	const auto msg = "No organism selected — click one in the world";
	ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(msg).x) * 0.5f);
	ImGui::TextDisabled("%s", msg);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Overview panel
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_overview(const SimSnapshot& snap, ImGuiContext& ctx, const OrganismTracker& protozoa)
{
	// ── Identity / Locomotion side-by-side ───────────────────────────────
	ImGui::Columns(2, nullptr, false);

	ImGui::TextDisabled("Identity");
	ImGui::Text("Cells   %d", protozoa.cell_count);
	ImGui::Text("Springs %d", protozoa.spring_count);

	ImGui::Spacing();
	ImGui::TextDisabled("Misc");
	ImGui::Text("Spring Work %.3f", protozoa.spring_total_work_done);
	ImGui::NextColumn();

	ImGui::TextDisabled("Locomotion");
	ImGui::Text("Speed %.4f", protozoa.speed);
	ImGui::Text("Vel X %.3f", protozoa.velocity.x);
	ImGui::Text("Vel Y %.3f", protozoa.velocity.y);
	ImGui::Spacing();
	ImGui::TextDisabled("Offspring");
	ImGui::Text("Count %d", protozoa.offspring_count);
	ImGui::Text("Food  %u", protozoa.total_food_eaten);

	ImGui::Columns(1);
	ImGui::Spacing();

	// ── Energy ───────────────────────────────────────────────────────────
	const float energy_f = std::clamp(protozoa.total_energy / protozoa.max_energy, 0.f, 1.f);
	char energy_lbl[32];
	snprintf(energy_lbl, sizeof(energy_lbl), "Energy %.0f / %.0f", protozoa.total_energy, protozoa.max_energy);
	colored_progress(energy_f, fraction_color(energy_f), energy_lbl, { -1, 15 });

	// ── Nutrients ───────
	ImGui::Spacing();
	const float nutrients_f = std::clamp(protozoa.total_nutrients / protozoa.max_nutrients, 0.f, 1.f);
	char nutrients_lbl[32];
	snprintf(nutrients_lbl, sizeof(nutrients_lbl), "Nutrients %.0f / %.0f", protozoa.total_nutrients, protozoa.max_nutrients);
	colored_progress(nutrients_f, nutrients_bar_col, nutrients_lbl, { -1, 15 });

	// ── Debug Overlays ────────────────────────────────────────────────────────
	ImGui::Spacing();
	ImGui::Separator();

	toggle(snap, ctx, "Debug Mode", &WorldToggles::debug_mode, "D");
	toggle(snap, ctx, "Bounding Boxes", &CellManagerToggles::show_bounding_boxes, "B");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Cells & Springs tab
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_cells_springs_tab(const SimSnapshot& snap, ImGuiContext& ctx, const OrganismTracker& protozoa)
{
	// fetching cell and spring container information
	const int cell_count = protozoa.cell_count;
	const int spring_count = protozoa.spring_count;

	// No need to show the list if both are empty
	if (cell_count == 0 && spring_count == 0)
	{
		ImGui::TextDisabled("No cells or springs.");
		return;
	}

	// Clamp selection indices to valid ranges, and switch selection type if the currently selected type is empty
	if (!cell_count == 0)
		m_sel_cell_idx_ = std::min(m_sel_cell_idx_, cell_count - 1);

	if (!spring_count == 0)
		m_sel_spring_idx_ = std::min(m_sel_spring_idx_, spring_count - 1);

	// If the currently selected type is empty, switch to the other type (if it's not empty)
	if (m_sel_is_spring_ && spring_count == 0)
		m_sel_is_spring_ = false;

	if (!m_sel_is_spring_ && cell_count == 0)
		m_sel_is_spring_ = true;

	// ── Unified selection list ────────────────────────────────────────────
	constexpr ImVec2 list_size = { 88.f, -1.f };
	ImGui::BeginChild("CS_list", list_size, true);


	for (int i = 0; i < cell_count; ++i)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, selector_color);
		ImGui::Text("●");
		ImGui::PopStyleColor();
		ImGui::SameLine();
		char lbl[12]; snprintf(lbl, sizeof(lbl), "C%d", i);
		if (ImGui::Selectable(lbl, !m_sel_is_spring_ && m_sel_cell_idx_ == i))
		{
			m_sel_cell_idx_ = i;
			m_sel_is_spring_ = false;
		}
	}

	if (!spring_count == 0)
	{
		ImGui::Separator();
		for (int i = 0; i < static_cast<int>(spring_count); ++i)
		{
			const Spring& si = protozoa.springs[i];
			char lbl[32]; snprintf(lbl, sizeof(lbl), "%d->%d##sp%d", si.cell_A_id, si.cell_B_id, i);
			if (ImGui::Selectable(lbl, m_sel_is_spring_ && m_sel_spring_idx_ == i))
			{
				m_sel_spring_idx_ = i;
				m_sel_is_spring_ = true;
			}
			//if (si.broken) ImGui::PopStyleColor();
		}
	}

	ImGui::EndChild();
	ImGui::SameLine();

	if (!m_sel_is_spring_ && !cell_count == 0)
	{
		const Cell& cell = protozoa.cells[m_sel_cell_idx_];
		const Body& body = protozoa.bodies[m_sel_cell_idx_];
		draw_cell_detail(ctx, cell, body);
	}

	else if (m_sel_is_spring_ && !spring_count == 0)
		draw_spring_detail(ctx, protozoa, protozoa.springs[m_sel_spring_idx_]);
}

void OrganismTab::draw_mutation_controls(const SimSnapshot& snap, ImGuiContext& ctx, const OrganismTracker& protozoa)
{
	ImGui::BeginChild("Mutation Controls", { -1.f, -1.f }, true);

	const OrganismTracker& p = snap.protozoa_tracker;
	const float sp = ImGui::GetStyle().ItemSpacing.x;
	const float total = ImGui::GetContentRegionAvail().x;
	const float cw = (total - sp * 2.f) / 3.f;
	constexpr float ch = -1.f;

	ImGui::Separator();
	static float tun_rate = 0.2f, tun_range = 0.2f;
	ImGui::SetNextItemWidth(-1.f); ImGui::SliderFloat("##tr", &tun_rate, 0.f, 1.f, "Rate  = %.3f");
	ImGui::SetNextItemWidth(-1.f); ImGui::SliderFloat("##trng", &tun_range, 0.f, 1.f, "Range = %.3f");
	ImGui::Spacing();
	if (ImGui::Button("Apply Mutation", { -1.f, 0.f }))
	{
		SimCommand cmd{ .section = CommandSection::CellManagerEvent, .type = CommandType::MutateProtozoa };
		cmd.mutate = { .mut_rate = tun_rate, .mut_range = tun_range };
		ctx.push(cmd);
	}

	ImGui::Spacing();
	ImGui::TextDisabled("Structure");
	ImGui::Separator();
	ImGui::Columns(2, nullptr, false);
	if (ImGui::Button("Add Cell", { -1.f, 0.f }))
		ctx.push({ .section = CommandSection::CellManagerEvent, .type = CommandType::AddCell });

	if (ImGui::Button("Remove Cell", { -1.f, 0.f }))
		ctx.push({ .section = CommandSection::CellManagerEvent, .type = CommandType::RemoveCell });

	ImGui::NextColumn();
	if (ImGui::Button("Add Spring", { -1.f, 0.f }))
		ctx.push({ .section = CommandSection::CellManagerEvent, .type = CommandType::AddSpring });

	if (ImGui::Button("Remove Spring", { -1.f, 0.f }))
		ctx.push({ .section = CommandSection::CellManagerEvent, .type = CommandType::RemoveSpring });

	ImGui::Columns(1);

	ImGui::Spacing();
	ImGui::Separator();

	static constexpr ModeOption<int> kStructureModes[] = {
	{ "For Selected##mode", 0 },
	{ "For All##mode",      1 },
	};

	mode_button_row(ctx, CommandSection::CellManagerEvent, CommandType::SetMouseMode,
		&SimCommand::int_val, kStructureModes, snap.world_stats.structure_mode,
		ImVec4{ 0.4f, 0.4f, 0.4f, 1.f }, 20);

	bool immortal_ = protozoa.cells[0].immortal_;
	if (ImGui::Checkbox("Immortal##org", &immortal_))
	{
		SimCommand cmd{ .section = CommandSection::CellManagerEvent, .type = CommandType::MakeImmortal };
		cmd.bool_val = immortal_;
		ctx.push(cmd);
	}
	ImGui::Spacing();
	if (ImGui::Button("Force Reproduce##org", { -1.f, 0.f }))
		ctx.push({ .section = CommandSection::CellManagerEvent, .type = CommandType::ForceReproduce });

	ImGui::Spacing();
	ImGui::PushStyleColor(ImGuiCol_Button, { 0.55f, 0.08f, 0.08f, 1.f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.75f, 0.15f, 0.15f, 1.f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 1.00f, 0.25f, 0.25f, 1.f });
	if (ImGui::Button("Force Die##org", { -1.f, 0.f }))
		ctx.push({ .section = CommandSection::CellManagerEvent, .type = CommandType::KillProtozoa });
	ImGui::PopStyleColor(3);

	if (ImGui::Button("Clone nearby##org", { -1.f, 0.f }))
		ctx.push({ .section = CommandSection::CellManagerEvent, .type = CommandType::CloneProtozoa });


	ImGui::EndChild();
}

void OrganismTab::draw_wave_panel(ImGuiContext& ctx, const float current_friction, const char* child_id,
	const char* description, int frames_alive, int idx, const char* value_label,
	std::vector<float>& scratch_buf,
	const WaveParam& amplitude, const WaveParam& frequency,
	const WaveParam& offset, const WaveParam& vertical_shift)
{
	const int period = safe_time_period(frequency.value, k_max_wave_buf);
	const int display_size = std::min(m_wave_cycles_ * period, k_max_wave_buf);
	const int head = frames_alive % display_size;

	ImGui::BeginChild(child_id, { 500, -1.f }, true);
	ImGui::TextDisabled("%s", description);

	scratch_buf.resize(display_size);
	PlotUtils::fill_sinwave(scratch_buf.data(), display_size,
		amplitude.value, frequency.value, offset.value, vertical_shift.value, 0.f, 1.f);
	PlotUtils::sinwave_graph("##wave", scratch_buf.data(), display_size, head, 0.f, 1.f, { -1.f, 52.f });
	ImGui::Text("t=%-4d  %s = %.4f", head, value_label, scratch_buf[head]);

	// Current friction
	const float fric = current_friction;
	const ImVec4 fc = { 1.f - fric, fric, 0.2f, 1.f };
	ImGui::Spacing();
	ImGui::TextDisabled("Friction");
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Text, fc);
	ImGui::Text("%.4f", fric);
	ImGui::PopStyleColor();
	colored_progress(fric, fc, "", { -1.f, 5.f });

	ImGui::Spacing();
	ImGui::SetNextItemWidth(-1.f);
	ImGui::SliderInt("##cycles", &m_wave_cycles_, 1, 8, "Display cycles = %d");

	const WaveParam params[] = { amplitude, frequency, offset, vertical_shift };
	for (int i = 0; i < 4; ++i)
	{
		ImGui::PushID(i); // needed since all four sliders now share the "##wp" label
		ImGui::Spacing();
		ImGui::SetNextItemWidth(-1.f);
		float val = params[i].value;
		if (ImGui::SliderFloat("##wp", &val, params[i].min, params[i].max, params[i].fmt))
		{
			SimCommand cmd{ .section = CommandSection::CellManagerEvent, .type = params[i].type };
			cmd.float_val = val;
			cmd.cell_spring_idx = idx;
			ctx.push(cmd);
		}
		ImGui::PopID();
	}

	ImGui::EndChild();
}
