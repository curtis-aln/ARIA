// Energy tab — history recording, composer (layout), and gauges card.
#include "energy_ui_common.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>

using namespace en_ui;

namespace
{
	// Drawn glyphs (the default font has no arrow characters): up / down triangle, or a flat dash.
	void draw_trend(ImDrawList* dl, ImVec2 c, float slope_per_tick)
	{
		const float per100 = slope_per_tick * 100.f;
		constexpr float s = 5.f;
		if (per100 > k_trend_eps)
			dl->AddTriangleFilled({ c.x, c.y - s }, { c.x - s, c.y + s }, { c.x + s, c.y + s }, u32(k_en_col_good));
		else if (per100 < -k_trend_eps)
			dl->AddTriangleFilled({ c.x - s, c.y - s }, { c.x + s, c.y - s }, { c.x, c.y + s }, u32(k_en_col_bad));
		else
			dl->AddRectFilled({ c.x - s, c.y - 1.f }, { c.x + s, c.y + 1.f }, u32(k_en_col_dim));
	}

	// Everything the cards need that can be derived from one tracker snapshot.
	EnergyStats instant_stats(const OrganismTracker& t)
	{
		EnergyStats st{};
		const int n = t.cell_count;
		st.n = n;
		if (n <= 0) return st;

		float thr_e = 0.f, thr_n = 0.f, thr_i = 0.f, tot_i = 0.f;
		float emin = FLT_MAX, emax = 0.f, esum = 0.f;
		for (int k = 0; k < n; ++k)
		{
			const Cell& c = t.cells[k];
			tot_i += c.get_integrity();
			thr_e += static_cast<float>(c.energy_threshold());
			thr_n += static_cast<float>(c.nutrients_threshold());
			thr_i += static_cast<float>(c.integrity_threshold());
			st.net_dE += c.delta_energy;
			st.net_dI += c.delta_integrity;

			const float ef = safe_frac(c.get_energy(), CellSettings::max_energy);
			emin = std::min(emin, ef);
			emax = std::max(emax, ef);
			esum += ef;
		}

		st.total_integrity = tot_i;
		st.max_integrity = CellSettings::max_integrity * static_cast<float>(n);

		st.frac[0] = safe_frac(t.total_energy, t.max_energy);
		st.frac[1] = safe_frac(t.total_nutrients, t.max_nutrients);
		st.frac[2] = safe_frac(tot_i, st.max_integrity);
		st.thr[0] = safe_frac(thr_e, t.max_energy);
		st.thr[1] = safe_frac(thr_n, t.max_nutrients);
		st.thr[2] = safe_frac(thr_i, st.max_integrity);

		st.cell_min = emin;
		st.cell_max = emax;
		st.cell_mean = esum / static_cast<float>(n);
		return st;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  History recording — called every frame from OrganismTab::draw()
//
//  x axis is a monotonic UI-side tick built from cells[0].internal_clock_ deltas,
//  so a paused sim adds nothing and removing cell 0 doesn't reset the graph.
//  Swap in a real sim tick from SimSnapshot if it has one.
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::record_energy_history(const SimSnapshot& snap)
{
	const OrganismTracker& t = snap.protozoa_tracker;

	if (t.selected_id != m_last_id_)
	{
		m_energy_hist_.clear();
		m_hist_last_clock_ = -1;
		m_hist_tick_ = 0.f;
		m_last_id_ = t.selected_id;
	}
	if (t.cell_count <= 0) return;

	const long long clock = static_cast<long long>(t.cells[0].internal_clock_);
	if (m_hist_last_clock_ >= 0)
	{
		if (clock == m_hist_last_clock_) return;                 // paused / no new tick
		const long long delta = clock - m_hist_last_clock_;
		m_hist_tick_ += delta > 0 ? static_cast<float>(delta) : 1.f;   // clock went backwards: cell list changed
	}
	m_hist_last_clock_ = clock;

	const EnergyStats st = instant_stats(t);
	EnergyHistory::Sample s{};
	s.tick = m_hist_tick_;
	s.v[EnergyHistory::Energy] = st.frac[0];
	s.v[EnergyHistory::Nutrients] = st.frac[1];
	s.v[EnergyHistory::Integrity] = st.frac[2];
	s.v[EnergyHistory::CellMin] = st.cell_min;
	s.v[EnergyHistory::CellMean] = st.cell_mean;
	s.v[EnergyHistory::CellMax] = st.cell_max;
	m_energy_hist_.push(s);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Energy tab — composer
//
//  One row of full-height cards:
//    Gauges | History (flex) | Budget | Cells (flex) | Feed | Constants (if wide enough)
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_energy_tab(ImGuiContext& ctx, const SimSnapshot& snap)
{
	const OrganismTracker& t = snap.protozoa_tracker;
	if (t.cell_count <= 0) { ImGui::TextDisabled("No cells."); return; }

	EnergyStats st = instant_stats(t);
	for (int k = 0; k < 3; ++k)
		st.slope[k] = m_energy_hist_.slope(static_cast<EnergyHistory::Series>(k), k_trend_window);

	// ── Width allocation ──────────────────────────────────────────────────
	const float sp = ImGui::GetStyle().ItemSpacing.x;
	const float total_w = ImGui::GetContentRegionAvail().x;
	constexpr float fixed3 = k_en_gauges_w + k_en_budget_w + k_en_feed_w;

	const bool show_consts = total_w >= fixed3 + k_en_consts_w + k_en_history_min_w + k_en_cells_min_w + sp * 5.f;
	const int  card_count = show_consts ? 6 : 5;
	const float fixed = fixed3 + (show_consts ? k_en_consts_w : 0.f);
	const float flex = std::max(total_w - fixed - sp * static_cast<float>(card_count - 1),
		k_en_history_min_w + k_en_cells_min_w);
	const float hist_w = std::floor(flex * k_en_history_share);
	const float cells_w = flex - hist_w;

	draw_energy_gauges(st, t);
	ImGui::SameLine();
	draw_energy_history(st, hist_w);
	ImGui::SameLine();
	draw_energy_budget(st, t, !show_consts);
	ImGui::SameLine();
	draw_energy_cells(t, cells_w);
	ImGui::SameLine();
	draw_energy_feed(ctx, st, t);

	if (show_consts)
	{
		ImGui::SameLine();
		if (begin_card("EN_consts", k_en_consts_w))
		{
			ImGui::TextDisabled("Constants");
			ImGui::Separator();
			draw_energy_constants();
		}
		ImGui::EndChild();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Gauges — three tall bars with reproduction-threshold ticks and trend arrows
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_energy_gauges(const EnergyStats& st, const OrganismTracker& t)
{
	if (begin_card("EN_gauges", k_en_gauges_w))
	{
		ImGui::TextDisabled("Organism  %d cells", st.n);
		ImGui::Separator();

		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		const ImVec2 avail = ImGui::GetContentRegionAvail();

		const float col_w = std::floor(avail.x / 3.f);
		const float top_h = 16.f;
		const float line_h = ImGui::GetTextLineHeight();
		const float bot_h = line_h * 2.f + 6.f;
		const float bar_h = std::max(avail.y - top_h - bot_h, 10.f);
		const float bar_w = std::min(col_w - 10.f, 34.f);

		static const char* names[3] = { "NRG", "NUT", "INT" };
		const float raws[3] = { t.total_energy, t.total_nutrients, st.total_integrity };
		const float maxs[3] = { t.max_energy, t.max_nutrients, st.max_integrity };
		const ImVec4 cols[3] = { fraction_color(st.frac[0]), k_en_col_nutrients, k_en_col_integrity };

		for (int k = 0; k < 3; ++k)
		{
			const float x0 = origin.x + col_w * static_cast<float>(k);
			const float cx = x0 + col_w * 0.5f;
			const float bar_top = origin.y + top_h;
			const float bar_bot = bar_top + bar_h;
			const ImVec2 b0 = { cx - bar_w * 0.5f, bar_top };
			const ImVec2 b1 = { cx + bar_w * 0.5f, bar_bot };

			draw_trend(dl, { cx, origin.y + top_h * 0.5f }, st.slope[k]);

			dl->AddRectFilled(b0, b1, u32({ 1.f, 1.f, 1.f, 0.06f }), 3.f);
			const float fh = bar_h * st.frac[k];
			if (fh > 0.5f)
				dl->AddRectFilled({ b0.x, bar_bot - fh }, b1, u32(cols[k]), 3.f);
			dl->AddRect(b0, b1, u32(k_en_col_card_border), 3.f);

			if (st.thr[k] > 0.f)
			{
				const float y = bar_bot - st.thr[k] * bar_h;
				dl->AddLine({ b0.x - 3.f, y }, { b1.x + 3.f, y }, u32({ 1.f, 1.f, 1.f, 0.9f }), 1.5f);
			}

			const ImVec2 ns = ImGui::CalcTextSize(names[k]);
			dl->AddText({ cx - ns.x * 0.5f, bar_bot + 3.f }, u32(k_en_col_dim), names[k]);
			char pct[16];
			std::snprintf(pct, sizeof pct, "%.0f%%", st.frac[k] * 100.f);
			const ImVec2 ps = ImGui::CalcTextSize(pct);
			dl->AddText({ cx - ps.x * 0.5f, bar_bot + 3.f + line_h }, u32({ 0.92f, 0.93f, 0.96f, 1.f }), pct);

			ImGui::SetCursorScreenPos({ x0, origin.y });
			ImGui::PushID(k);
			ImGui::InvisibleButton("##gauge", { col_w, top_h + bar_h + bot_h });
			ImGui::PopID();
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::TextUnformatted(names[k]);
				ImGui::Text("%.1f / %.0f  (%.0f%%)", raws[k], maxs[k], st.frac[k] * 100.f);
				if (st.thr[k] > 0.f)
					ImGui::Text("repro threshold: %.1f  (%.0f%%)", st.thr[k] * maxs[k], st.thr[k] * 100.f);
				ImGui::Text("trend: %+.2f / 100 fr", st.slope[k] * 100.f * maxs[k]);
				ImGui::EndTooltip();
			}
		}
	}
	ImGui::EndChild();
}