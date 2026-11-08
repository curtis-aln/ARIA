#include "../graphs/plot_utils.h"
#include "organism_tab.h"
#include <algorithm>
#include <cstdio>
#include <entities/body/body.h>
#include <entities/cell/cell.h>
#include <entities/spring/spring.h>
#include <imgui.h>
#include <managers/cell_manager/organism_tracker/organism_tracker.h>
#include <simulation/context/sim_command.h>
#include <simulation/context/sim_snapshot.h>
#include <simulation/context/state.h>
#include <simulation/imgui/tabs/i_tab.h>
#include <vector>
#include <limits>
#include <numbers>
#include <numeric>

namespace
{
	constexpr float k_list_w = 132.f;
	constexpr float k_row_h = 22.f;

	constexpr float k_swatch = 12.f;
	constexpr float k_swatch_mute = 0.70f;   // 1 = original colour, 0 = fully blended into the row bg

	// Pulls a genome colour toward the dark row background so the chip isn't loud.
	ImU32 muted_u32(const sf::Color& c)
	{
		constexpr float bg[3] = { 0.15f, 0.16f, 0.19f };
		auto mix = [](float ch, float b) { return (ch / 255.f) * k_swatch_mute + b * (1.f - k_swatch_mute); };
		return ImGui::ColorConvertFloat4ToU32({ mix(c.r, bg[0]), mix(c.g, bg[1]), mix(c.b, bg[2]), 1.f });
	}

	// Full-width clickable row: dark neutral background, small split colour chip on the left.
	// `upper` fills the chip's top-left triangle, `lower` the bottom-right (same colour twice = flat).
	bool colour_row(const char* id, const char* text, const sf::Color& upper, const sf::Color& lower, bool selected)
	{
		const float w = ImGui::GetContentRegionAvail().x;
		const ImVec2 p0 = ImGui::GetCursorScreenPos();
		const ImVec2 p1 = { p0.x + w, p0.y + k_row_h };

		const bool pressed = ImGui::InvisibleButton(id, { w, k_row_h });
		const bool hovered = ImGui::IsItemHovered();

		ImDrawList* dl = ImGui::GetWindowDrawList();

		const ImVec4 bg = selected ? ImVec4{ 0.24f, 0.26f, 0.32f, 1.f }
			: hovered ? ImVec4{ 0.20f, 0.21f, 0.26f, 1.f }
		: ImVec4{ 0.15f, 0.16f, 0.19f, 1.f };
		dl->AddRectFilled(p0, p1, ImGui::ColorConvertFloat4ToU32(bg), 3.f);
		if (selected)
			dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 90), 3.f, 0, 1.f);

		// Colour chip
		const float sx0 = p0.x + 6.f, sx1 = sx0 + k_swatch;
		const float sy0 = p0.y + (k_row_h - k_swatch) * 0.5f, sy1 = sy0 + k_swatch;
		dl->AddTriangleFilled({ sx0, sy0 }, { sx1, sy0 }, { sx0, sy1 }, muted_u32(upper));
		dl->AddTriangleFilled({ sx1, sy0 }, { sx1, sy1 }, { sx0, sy1 }, muted_u32(lower));
		dl->AddRect({ sx0, sy0 }, { sx1, sy1 }, IM_COL32(0, 0, 0, 120), 0.f, 0, 1.f);

		const ImVec2 tp = { sx1 + 8.f, p0.y + (k_row_h - ImGui::GetTextLineHeight()) * 0.5f };
		dl->AddText(tp, selected ? IM_COL32(217, 222, 235, 255) : IM_COL32(178, 184, 199, 255), text);
		return pressed;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Top-level draw
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw(const SimSnapshot& snap, ImGuiContext& ctx)
{
	const OrganismTracker& protozoa = snap.protozoa_tracker;

	if (protozoa.selected_id == -1)
	{
		push_highlight(ctx, k_no_key);   // clear the world-side circle
		m_sel_org_id_ = -1;
		draw_no_selection();
		return;
	}

	record_energy_history(snap);
	rebuild_ordering(protozoa);

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

	if (ImGui::BeginTabItem("Cells & Springs"))
	{
		draw_cells_springs_tab(snap, ctx, protozoa);
		ImGui::SameLine();
		draw_mutation_controls(snap, ctx, protozoa);
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem("Energy")) { draw_energy_tab(ctx, snap); ImGui::EndTabItem(); }
	if (ImGui::BeginTabItem("Brain")) { Brain();                    ImGui::EndTabItem(); }
	ImGui::PopStyleVar();

	ImGui::EndTabBar();
	ImGui::EndChild();

	sync_selection(ctx, protozoa);   // after the UI, so clicks in any tab are captured
}

// ─────────────────────────────────────────────────────────────────────────────
//  Selection stability: sort by ID, re-resolve the selected index from its stored key
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::rebuild_ordering(const OrganismTracker& t)
{
	if (t.selected_id != m_sel_org_id_)   // genuinely different organism: reset
	{
		m_sel_org_id_ = t.selected_id;
		m_sel_cell_key_ = m_sel_spring_key_ = k_no_key;
		m_sel_is_spring_ = false;
	}

	const int nc = t.cell_count;
	const int ns = static_cast<int>(t.tendon_count);

	m_cell_order_.resize(static_cast<size_t>(nc));
	std::iota(m_cell_order_.begin(), m_cell_order_.end(), 0);
	std::stable_sort(m_cell_order_.begin(), m_cell_order_.end(),
		[&](int a, int b) { return cell_key(t.cells[a]) < cell_key(t.cells[b]); });

	m_cell_rank_.assign(static_cast<size_t>(nc), 0);
	m_sorted_cell_keys_.resize(static_cast<size_t>(nc));
	for (int k = 0; k < nc; ++k)
	{
		m_cell_rank_[m_cell_order_[k]] = k;
		m_sorted_cell_keys_[k] = cell_key(t.cells[m_cell_order_[k]]);
	}

	m_spring_order_.resize(static_cast<size_t>(ns));
	std::iota(m_spring_order_.begin(), m_spring_order_.end(), 0);
	std::stable_sort(m_spring_order_.begin(), m_spring_order_.end(),
		[&](int a, int b) { return spring_key(t.springs[a]) < spring_key(t.springs[b]); });

	m_spring_rank_.assign(static_cast<size_t>(ns), 0);
	std::vector<uint32_t> spring_keys(static_cast<size_t>(ns));
	for (int k = 0; k < ns; ++k)
	{
		m_spring_rank_[m_spring_order_[k]] = k;
		spring_keys[k] = spring_key(t.springs[m_spring_order_[k]]);
	}

	// Resolve stored keys back to tracker indices (fall back to the first entry).
	m_sel_cell_idx_ = nc > 0 ? m_cell_order_[0] : 0;
	if (m_sel_cell_key_ != k_no_key)
	{
		const auto it = std::lower_bound(m_sorted_cell_keys_.begin(), m_sorted_cell_keys_.end(), m_sel_cell_key_);
		if (it != m_sorted_cell_keys_.end() && *it == m_sel_cell_key_)
			m_sel_cell_idx_ = m_cell_order_[static_cast<size_t>(it - m_sorted_cell_keys_.begin())];
	}

	m_sel_spring_idx_ = ns > 0 ? m_spring_order_[0] : 0;
	if (m_sel_spring_key_ != k_no_key)
	{
		const auto it = std::lower_bound(spring_keys.begin(), spring_keys.end(), m_sel_spring_key_);
		if (it != spring_keys.end() && *it == m_sel_spring_key_)
			m_sel_spring_idx_ = m_spring_order_[static_cast<size_t>(it - spring_keys.begin())];
	}
}

void OrganismTab::sync_selection(ImGuiContext& ctx, const OrganismTracker& t)
{
	const int ns = static_cast<int>(t.tendon_count);
	if (t.cell_count > 0)
	{
		m_sel_cell_idx_ = std::clamp(m_sel_cell_idx_, 0, t.cell_count - 1);
		m_sel_cell_key_ = cell_key(t.cells[m_sel_cell_idx_]);
	}
	if (ns > 0)
	{
		m_sel_spring_idx_ = std::clamp(m_sel_spring_idx_, 0, ns - 1);
		m_sel_spring_key_ = spring_key(t.springs[m_sel_spring_idx_]);
	}
	push_highlight(ctx, (!m_sel_is_spring_ && t.cell_count > 0) ? m_sel_cell_key_ : k_no_key);
}

void OrganismTab::push_highlight(ImGuiContext& ctx, const uint32_t key)
{
	if (key == m_pushed_highlight_) return;   // only talk to the sim on change
	m_pushed_highlight_ = key;
	SimCommand cmd{ .section = CommandSection::CellManagerEvent, .type = CommandType::SetHighlightedCell };
	cmd.int_val = key == k_no_key ? -1 : static_cast<int>(key);
	ctx.push(cmd);
}

int OrganismTab::relative_cell_index(const uint32_t abs_id) const
{
	const auto it = std::lower_bound(m_sorted_cell_keys_.begin(), m_sorted_cell_keys_.end(), abs_id);
	return (it != m_sorted_cell_keys_.end() && *it == abs_id)
		? static_cast<int>(it - m_sorted_cell_keys_.begin()) : -1;
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
	ImGui::Text("Springs %d", protozoa.tendon_count);

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

	if (snap.world_toggles.debug_mode)   // ← adjust to however the snapshot exposes toggle state
	{
		ImGui::Indent();
		toggle(snap, ctx, "Protozoa Bounding Box", &CellManagerToggles::show_bounding_boxes, "B");
		toggle(snap, ctx, "Cell Bounding Boxes", &CellManagerToggles::show_cell_bounding_boxes, "");
		toggle(snap, ctx, "Show Nearby", &CellManagerToggles::show_nearby, "");
		ImGui::Unindent();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Cells & Springs tab
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_cells_springs_tab(const SimSnapshot& snap, ImGuiContext& ctx, const OrganismTracker& protozoa)
{
	const int cell_count = protozoa.cell_count;
	const int tendon_count = static_cast<int>(protozoa.tendon_count);

	if (cell_count == 0 && tendon_count == 0)
	{
		ImGui::TextDisabled("No cells or springs.");
		return;
	}

	if (m_sel_is_spring_ && tendon_count == 0) m_sel_is_spring_ = false;
	if (!m_sel_is_spring_ && cell_count == 0)  m_sel_is_spring_ = true;

	ImGui::BeginChild("CS_list", { k_list_w, -1.f }, true);

	// Cells: top-left triangle = outer colour, bottom-right = inner colour.
	for (int k = 0; k < cell_count; ++k)
	{
		const int i = m_cell_order_[k];
		const Cell& c = protozoa.cells[i];
		char id[16], lbl[24];
		snprintf(id, sizeof id, "##cell%d", k);
		snprintf(lbl, sizeof lbl, "Cell %d", k);
		if (colour_row(id, lbl, c.get_outer_color(), c.get_inner_color(), !m_sel_is_spring_ && m_sel_cell_idx_ == i))
		{
			m_sel_cell_idx_ = i;
			m_sel_is_spring_ = false;
		}
	}

	// Springs: relative numbering, single flat colour.
	if (tendon_count > 0)
	{
		ImGui::Separator();
		ImGui::TextDisabled("Springs");
		for (int k = 0; k < tendon_count; ++k)
		{
			const int i = m_spring_order_[k];
			const Tendon& s = protozoa.springs[i];

			char a[8], b[8], id[16], lbl[40];
			const int ra = relative_cell_index(s.cell_A_id), rb = relative_cell_index(s.cell_B_id);
			if (ra >= 0) snprintf(a, sizeof a, "%d", ra); else snprintf(a, sizeof a, "?");
			if (rb >= 0) snprintf(b, sizeof b, "%d", rb); else snprintf(b, sizeof b, "?");
			snprintf(id, sizeof id, "##spr%d", k);
			snprintf(lbl, sizeof lbl, "Spring %d: %s-%s", k, a, b);

			const sf::Color col(s.genome.outer_r, s.genome.outer_g, s.genome.outer_b);
			if (colour_row(id, lbl, col, col, m_sel_is_spring_ && m_sel_spring_idx_ == i))
			{
				m_sel_spring_idx_ = i;
				m_sel_is_spring_ = true;
			}
		}
	}

	ImGui::EndChild();
	ImGui::SameLine();

	if (!m_sel_is_spring_ && cell_count > 0)
		draw_cell_detail(ctx, protozoa.cells[m_sel_cell_idx_], protozoa.bodies[m_sel_cell_idx_]);
	else if (m_sel_is_spring_ && tendon_count > 0)
		draw_spring_detail(ctx, protozoa, protozoa.springs[m_sel_spring_idx_], m_spring_rank_[m_sel_spring_idx_]);
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
		ctx.push({ .section = CommandSection::CellManagerEvent, .type = CommandType::AddTendon });

	if (ImGui::Button("Remove Spring", { -1.f, 0.f }))
		ctx.push({ .section = CommandSection::CellManagerEvent, .type = CommandType::RemoveTendon });

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
	// Same float math and same fast_sin as Cell::calculate_sin_output / Spring::calculate_rest_length.
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
			cmd.cell_tendon_idx = idx;
			ctx.push(cmd);
		}
		ImGui::PopID();
	}

	ImGui::EndChild();
}