#include "../graphs/plot_utils.h"
#include "organism_tab.h"
#include <algorithm>
#include <cstdio>
#include <entities/body/body.h>
#include <entities/cell/cell.h>
#include <entities/spring/spring.h>
#include <imgui.h>
#include <managers/cell_manager/organism_tracker.h>
#include <simulation/context/sim_command.h>
#include <simulation/context/sim_snapshot.h>
#include <simulation/context/state.h>
#include <simulation/imgui/tabs/i_tab.h>
#include <vector>
#include <limits>
#include <numbers>



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

void OrganismTab::draw_wave_panel(ImGuiContext& ctx, const char* child_id, const char* description,
	const double now, const int idx, const char* value_label, const float actual_value, const ImVec4 col,
	const WaveParam& amplitude, const WaveParam& frequency,
	const WaveParam& offset, const WaveParam& vertical_shift)
{
	const double A = amplitude.value, B = frequency.value, D = vertical_shift.value;
	// Same float math and same fast_sin as Cell::calculate_friction / Spring::calculate_rest_length.
	const auto raw_at = [fA = amplitude.value, fB = frequency.value,
		fC = offset.value, fD = vertical_shift.value](double t)
		{
			return static_cast<double>(fD + fA * fast_sin(fB * static_cast<float>(t) + fC));
		};
	const auto wave_at = [&](double t) { return std::clamp(raw_at(t), 0.0, 1.0); };

	// Exact (unrounded) period, so N display cycles is exactly N cycles.
	constexpr double k_max_span = 100000.0;
	const double period = std::abs(B) > 1e-9 ? 2.0 * std::numbers::pi / std::abs(B) : k_max_span;
	const double span = std::min(period * m_wave_cycles_, k_max_span);

	// Window follows "now"; never shows frames before birth.
	const double x_min = std::max(0.0, now - span * 0.5);
	const double x_max = x_min + span;

	constexpr int N = 512;
	static double xs[N], ys[N], raw[N];
	for (int i = 0; i < N; ++i)
	{
		xs[i] = x_min + span * i / (N - 1);
		raw[i] = raw_at(xs[i]);
		ys[i] = std::clamp(raw[i], 0.0, 1.0);
	}

	const double raw_lo = D - std::abs(A), raw_hi = D + std::abs(A);
	const bool   clipped = raw_hi > 1.0 || raw_lo < 0.0;
	const double y_lo = std::min(0.0, raw_lo) - 0.06;
	const double y_hi = std::max(1.0, raw_hi) + 0.06;

	ImGui::BeginChild(child_id, { 500, -1.f }, true);
	ImGui::TextDisabled("%s", description);

	ImPlot::PushStyleColor(ImPlotCol_FrameBg, { 0.f, 0.f, 0.f, 0.f });
	ImPlot::PushStyleColor(ImPlotCol_PlotBg, { 0.f, 0.f, 0.f, 0.f });
	ImPlot::PushStyleColor(ImPlotCol_PlotBorder, { 0.f, 0.f, 0.f, 0.f });
	ImPlot::PushStyleColor(ImPlotCol_AxisGrid, { 1.f, 1.f, 1.f, 0.07f });
	ImPlot::PushStyleColor(ImPlotCol_AxisText, { 0.55f, 0.58f, 0.65f, 1.f });
	ImPlot::PushStyleVar(ImPlotStyleVar_PlotBorderSize, 0.f);

	constexpr ImPlotFlags pf = ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect |
		ImPlotFlags_NoMouseText | ImPlotFlags_NoLegend | ImPlotFlags_NoInputs;

	if (ImPlot::BeginPlot("##wave", { -1.f, 80.f }, pf))
	{
		static const double y_ticks[] = { 0.0, 0.5, 1.0 };
		ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoTickMarks, ImPlotAxisFlags_NoTickMarks);
		ImPlot::SetupAxisLimits(ImAxis_X1, x_min, x_max, ImGuiCond_Always);
		ImPlot::SetupAxisLimits(ImAxis_Y1, y_lo, y_hi, ImGuiCond_Always);
		ImPlot::SetupAxisTicks(ImAxis_Y1, y_ticks, 3);

		// Valid [0,1] band
		const double bx[2] = { x_min, x_max }, by[2] = { 1.0, 1.0 };
		ImPlot::SetNextFillStyle({ 1.f, 1.f, 1.f, 1.f }, 0.04f);
		ImPlot::PlotShaded("##valid", bx, by, 2, 0.0);

		// Unclamped wave, faint, only when the clamp is actually cutting it
		if (clipped)
		{
			ImPlot::SetNextLineStyle({ col.x, col.y, col.z, 0.30f }, 1.f);
			ImPlot::PlotLine("##raw", xs, raw, N);
		}

		ImPlot::SetNextFillStyle(col, 0.12f);
		ImPlot::PlotShaded("##fill", xs, ys, N, 0.0);
		ImPlot::SetNextLineStyle(col, 2.f);
		ImPlot::PlotLine("##curve", xs, ys, N);

		// "Now": line + model ring + actual dot (when the real value is known)
		const double nx = now, model_y = wave_at(now);
		const double actual_y = actual_value;
		const bool known = !std::isnan(actual_value);

		ImPlot::SetNextLineStyle({ 1.f, 1.f, 1.f, 0.30f }, 1.f);
		ImPlot::PlotInfLines("##now", &nx, 1);

		if (known)
		{
			ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 8.f, { 0.f, 0.f, 0.f, 0.f }, 1.5f, { 1.f, 1.f, 1.f, 0.9f });
			ImPlot::PlotScatter("##model", &nx, &model_y, 1);
			ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 4.f, col, IMPLOT_AUTO, col);
			ImPlot::PlotScatter("##actual", &nx, &actual_y, 1);
		}
		else
		{
			ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 5.f, { 1.f, 1.f, 1.f, 1.f }, IMPLOT_AUTO, { 1.f, 1.f, 1.f, 1.f });
			ImPlot::PlotScatter("##model", &nx, &model_y, 1);
		}
		ImPlot::EndPlot();
	}
	ImPlot::PopStyleVar(1);
	ImPlot::PopStyleColor(5);

	// ── Readout ───────────────────────────────────────────────────────────
	const double model_v = wave_at(now);
	ImGui::TextDisabled("frame %.0f  period %.1f fr", now, period);
	ImGui::SameLine(0.f, 14.f);
	ImGui::TextColored(col, "%s", value_label);
	ImGui::SameLine();
	if (!std::isnan(actual_value))
	{
		// tolerance covers the real value being read one tick before/after our clock
		const bool mismatch = std::abs(actual_value - model_v) > 1e-3;
		ImGui::Text("%.4f", actual_value);
		ImGui::SameLine(0.f, 10.f);
		if (mismatch) ImGui::TextColored({ 1.f, 0.35f, 0.35f, 1.f }, "model %.4f MISMATCH", model_v);
		else          ImGui::TextDisabled("model %.4f", model_v);
		colored_progress(actual_value, col, "", { -1.f, 4.f });
	}
	else
		ImGui::Text("%.4f (model)", model_v);

	ImGui::Spacing();
	ImGui::SetNextItemWidth(-1.f);
	ImGui::SliderInt("##cycles", &m_wave_cycles_, 1, 8, "Display cycles = %d");

	// ── Genome sliders, 2x2 ───────────────────────────────────────────────
	const WaveParam params[] = { amplitude, frequency, offset, vertical_shift };
	const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
	for (int i = 0; i < 4; ++i)
	{
		if (i % 2) ImGui::SameLine();
		ImGui::PushID(i);
		ImGui::SetNextItemWidth(half);
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
