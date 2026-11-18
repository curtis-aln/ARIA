// Energy tab: history recording, card layout, and the organism gauges card.
#include "energy_ui_common.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>

using namespace en_ui;

namespace
{
	// ── Gauge slots ──────────────────────────────────────────────────────────
	// One slot per gauge. The same index is used for EnergyStats' per-gauge arrays and for
	// EnergyHistory::Series, so the order here must match the history enum.
	enum GaugeSlot { SlotEnergy = 0, SlotNutrients = 1, SlotIntegrity = 2, SlotCount = 3 };

	static_assert(static_cast<int>(EnergyHistory::Energy) == SlotEnergy
		&& static_cast<int>(EnergyHistory::Nutrients) == SlotNutrients
		&& static_cast<int>(EnergyHistory::Integrity) == SlotIntegrity,
		"GaugeSlot order must match EnergyHistory::Series");

	// ── Trend glyph ──────────────────────────────────────────────────────────
	// Drawn by hand because the default font has no arrow characters:
	// green up-triangle (rising), red down-triangle (falling), or a flat dash (steady).
	void draw_trend_glyph(ImDrawList* draw_list, const ImVec2 center, const float slope_per_tick)
	{
		const float slope_per_100_ticks = slope_per_tick * 100.f;
		constexpr float half = 5.f;

		if (slope_per_100_ticks > k_trend_eps)
			draw_list->AddTriangleFilled({ center.x, center.y - half }, { center.x - half, center.y + half },
				{ center.x + half, center.y + half }, u32(k_en_col_good));
		else if (slope_per_100_ticks < -k_trend_eps)
			draw_list->AddTriangleFilled({ center.x - half, center.y - half }, { center.x + half, center.y - half },
				{ center.x, center.y + half }, u32(k_en_col_bad));
		else
			draw_list->AddRectFilled({ center.x - half, center.y - 1.f }, { center.x + half, center.y + 1.f },
				u32(k_en_col_dim));
	}

	// ── Instant stats ────────────────────────────────────────────────────────
	// Everything the cards need that can be derived from a single tracker snapshot
	// (no history involved, so trend slopes are filled in separately by the caller).
	EnergyStats compute_instant_stats(const OrganismTracker& tracker)
	{
		EnergyStats stats{};
		const int cell_count = tracker.cell_count;
		stats.cell_count = cell_count;
		if (cell_count <= 0)
			return stats;

		// Organism-wide sums, in absolute units
		float total_integrity = 0.f;
		float max_integrity = 0.f;            // sum of every cell's own integrity cap
		float energy_threshold_sum = 0.f;     // reproduction thresholds, absolute
		float nutrient_threshold_sum = 0.f;
		float integrity_threshold_sum = 0.f;

		// Spread of per-cell energy fill (0..1)
		float lowest_energy_fill = FLT_MAX;
		float highest_energy_fill = 0.f;
		float energy_fill_sum = 0.f;

		for (int i = 0; i < cell_count; ++i)
		{
			const Cell& cell = tracker.cells[i];

			total_integrity += cell.get_integrity();
			max_integrity += cell.max_integrity;   // rename to your accessor

			energy_threshold_sum += static_cast<float>(cell.energy_threshold());
			nutrient_threshold_sum += static_cast<float>(cell.nutrients_threshold());
			integrity_threshold_sum += static_cast<float>(cell.integrity_threshold());

			stats.net_energy_delta += cell.delta_energy;
			stats.net_integrity_delta += cell.delta_integrity;

			const float energy_fill = safe_frac(cell.get_energy(), cell.max_energy);
			lowest_energy_fill = std::min(lowest_energy_fill, energy_fill);
			highest_energy_fill = std::max(highest_energy_fill, energy_fill);
			energy_fill_sum += energy_fill;
		}

		stats.total_integrity = total_integrity;
		stats.max_integrity = max_integrity;

		// How full the organism is, per gauge
		stats.fill_fraction[SlotEnergy] = safe_frac(tracker.total_energy, tracker.max_energy);
		stats.fill_fraction[SlotNutrients] = safe_frac(tracker.total_nutrients, tracker.max_nutrients);
		stats.fill_fraction[SlotIntegrity] = safe_frac(total_integrity, max_integrity);

		// Where reproduction kicks in, as a fraction of each gauge's capacity
		stats.repro_threshold_fraction[SlotEnergy] = safe_frac(energy_threshold_sum, tracker.max_energy);
		stats.repro_threshold_fraction[SlotNutrients] = safe_frac(nutrient_threshold_sum, tracker.max_nutrients);
		stats.repro_threshold_fraction[SlotIntegrity] = safe_frac(integrity_threshold_sum, max_integrity);

		stats.cell_energy_min = lowest_energy_fill;
		stats.cell_energy_max = highest_energy_fill;
		stats.cell_energy_mean = energy_fill_sum / static_cast<float>(cell_count);
		return stats;
	}

	// ── Card layout ──────────────────────────────────────────────────────────
	struct EnergyLayout
	{
		bool  show_constants;   // the Constants card only appears when there is room for it
		float history_w;        // width of the flexible History card
		float cells_w;          // width of the flexible Cells card
	};

	// Row order: Gauges | History (flex) | Budget | Cells (flex) | Feed | Constants (if wide enough).
	// Fixed-width cards take what they need, and the two flexible cards split whatever is left.
	EnergyLayout plan_energy_layout(const float total_w, const float item_spacing)
	{
		constexpr float fixed_core_w = k_en_gauges_w + k_en_budget_w + k_en_feed_w;

		EnergyLayout layout{};
		layout.show_constants = total_w >= fixed_core_w + k_en_consts_w + k_en_history_min_w
			+ k_en_cells_min_w + item_spacing * 5.f;

		const int   card_count = layout.show_constants ? 6 : 5;
		const float fixed_w = fixed_core_w + (layout.show_constants ? k_en_consts_w : 0.f);
		const float flexible_w = std::max(total_w - fixed_w - item_spacing * static_cast<float>(card_count - 1),
			k_en_history_min_w + k_en_cells_min_w);

		layout.history_w = std::floor(flexible_w * k_en_history_share);
		layout.cells_w = flexible_w - layout.history_w;
		return layout;
	}

	// ── Gauge drawing ────────────────────────────────────────────────────────
	// What one gauge column needs in order to draw itself and its tooltip.
	struct GaugeView
	{
		const char* label;            // "NRG" / "NUT" / "INT"
		float       value;            // organism total, absolute units
		float       capacity;         // organism capacity, absolute units
		float       fill;             // value / capacity, 0..1
		float       threshold;        // reproduction threshold as a fraction of capacity (0 = none)
		float       slope_per_tick;   // recent trend, in fill fraction per tick
		ImVec4      color;
	};

	// Shared pixel geometry for every gauge column.
	struct GaugeGeometry
	{
		float column_w;        // width of one gauge column
		float trend_row_h;     // height of the trend glyph row above the bar
		float bar_h;
		float bar_w;
		float label_block_h;   // name line + percent line below the bar
		float text_h;          // one line of text
	};

	void draw_gauge_column(ImDrawList* draw_list, const int id, const GaugeView& gauge,
		const GaugeGeometry& geo, const float column_x, const float top_y)
	{
		const float center_x = column_x + geo.column_w * 0.5f;
		const float bar_top = top_y + geo.trend_row_h;
		const float bar_bottom = bar_top + geo.bar_h;
		const ImVec2 bar_min = { center_x - geo.bar_w * 0.5f, bar_top };
		const ImVec2 bar_max = { center_x + geo.bar_w * 0.5f, bar_bottom };

		draw_trend_glyph(draw_list, { center_x, top_y + geo.trend_row_h * 0.5f }, gauge.slope_per_tick);

		// Bar: empty track, filled portion, outline
		draw_list->AddRectFilled(bar_min, bar_max, u32({ 1.f, 1.f, 1.f, 0.06f }), 3.f);
		const float fill_h = geo.bar_h * gauge.fill;
		if (fill_h > 0.5f)
			draw_list->AddRectFilled({ bar_min.x, bar_bottom - fill_h }, bar_max, u32(gauge.color), 3.f);
		draw_list->AddRect(bar_min, bar_max, u32(k_en_col_card_border), 3.f);

		// Reproduction threshold tick, drawn slightly wider than the bar
		if (gauge.threshold > 0.f)
		{
			const float tick_y = bar_bottom - gauge.threshold * geo.bar_h;
			draw_list->AddLine({ bar_min.x - 3.f, tick_y }, { bar_max.x + 3.f, tick_y }, u32({ 1.f, 1.f, 1.f, 0.9f }), 1.5f);
		}

		// Labels under the bar: name, then percent full
		const ImVec2 label_size = ImGui::CalcTextSize(gauge.label);
		draw_list->AddText({ center_x - label_size.x * 0.5f, bar_bottom + 3.f }, u32(k_en_col_dim), gauge.label);

		char percent_text[16];
		std::snprintf(percent_text, sizeof percent_text, "%.0f%%", gauge.fill * 100.f);
		const ImVec2 percent_size = ImGui::CalcTextSize(percent_text);
		draw_list->AddText({ center_x - percent_size.x * 0.5f, bar_bottom + 3.f + geo.text_h },
			u32({ 0.92f, 0.93f, 0.96f, 1.f }), percent_text);

		// Hover area covers the whole column (trend glyph, bar, labels)
		ImGui::SetCursorScreenPos({ column_x, top_y });
		ImGui::PushID(id);
		ImGui::InvisibleButton("##gauge", { geo.column_w, geo.trend_row_h + geo.bar_h + geo.label_block_h });
		ImGui::PopID();

		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(gauge.label);
			ImGui::Text("%.1f / %.0f  (%.0f%%)", gauge.value, gauge.capacity, gauge.fill * 100.f);
			if (gauge.threshold > 0.f)
				ImGui::Text("repro threshold: %.1f  (%.0f%%)", gauge.threshold * gauge.capacity, gauge.threshold * 100.f);
			ImGui::Text("trend: %+.2f / 100 fr", gauge.slope_per_tick * 100.f * gauge.capacity);
			ImGui::EndTooltip();
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  History recording, called every frame from OrganismTab::draw()
//
//  The x axis is a monotonic UI-side tick built from cells[0].internal_clock_ deltas,
//  so a paused sim adds nothing and removing cell 0 doesn't reset the graph.
//  Swap in a real sim tick from SimSnapshot if it has one.
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::record_energy_history(const SimSnapshot& snapshot)
{
	const OrganismTracker& tracker = snapshot.protozoa_tracker;

	// A different organism was selected: start a fresh graph
	if (tracker.selected_id != m_history_organism_id_)
	{
		m_energy_history_.clear();
		m_history_last_clock_ = -1;
		m_history_tick_ = 0.f;
		m_history_organism_id_ = tracker.selected_id;
	}
	if (tracker.cell_count <= 0)
		return;

	const long long clock = static_cast<long long>(tracker.cells[0].internal_clock_);
	if (m_history_last_clock_ >= 0)
	{
		if (clock == m_history_last_clock_)
			return;                                   // paused, no new tick

		const long long clock_delta = clock - m_history_last_clock_;
		// A negative delta means the cell list changed under us; count it as one tick
		m_history_tick_ += clock_delta > 0 ? static_cast<float>(clock_delta) : 1.f;
	}
	m_history_last_clock_ = clock;

	const EnergyStats stats = compute_instant_stats(tracker);
	EnergyHistory::Sample sample{};
	sample.tick = m_history_tick_;
	sample.v[EnergyHistory::Energy] = stats.fill_fraction[SlotEnergy];
	sample.v[EnergyHistory::Nutrients] = stats.fill_fraction[SlotNutrients];
	sample.v[EnergyHistory::Integrity] = stats.fill_fraction[SlotIntegrity];
	sample.v[EnergyHistory::CellMin] = stats.cell_energy_min;
	sample.v[EnergyHistory::CellMean] = stats.cell_energy_mean;
	sample.v[EnergyHistory::CellMax] = stats.cell_energy_max;
	m_energy_history_.push(sample);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Energy tab composer
//
//  One row of full-height cards:
//    Gauges | History (flex) | Budget | Cells (flex) | Feed | Constants (if wide enough)
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_energy_tab(ImGuiContext& ctx, const SimSnapshot& snapshot)
{
	const OrganismTracker& tracker = snapshot.protozoa_tracker;
	if (tracker.cell_count <= 0)
	{
		ImGui::TextDisabled("No cells.");
		return;
	}

	// Instant stats from this frame's snapshot, plus recent trends from the recorded history
	EnergyStats stats = compute_instant_stats(tracker);
	for (int slot = 0; slot < SlotCount; ++slot)
		stats.slope_per_tick[slot] = m_energy_history_.slope(static_cast<EnergyHistory::Series>(slot), k_trend_window);

	const EnergyLayout layout = plan_energy_layout(ImGui::GetContentRegionAvail().x, ImGui::GetStyle().ItemSpacing.x);

	draw_energy_gauges(stats, tracker);
	ImGui::SameLine();
	draw_energy_history(stats, layout.history_w);
	ImGui::SameLine();
	draw_energy_budget(stats, tracker, !layout.show_constants);   // budget goes compact when Constants is hidden
	ImGui::SameLine();
	draw_energy_cells(tracker, layout.cells_w);
	ImGui::SameLine();
	draw_energy_feed(ctx, stats, tracker);

	if (layout.show_constants)
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
//  Gauges card: three tall bars with reproduction-threshold ticks and trend arrows
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_energy_gauges(const EnergyStats& stats, const OrganismTracker& tracker)
{
	if (begin_card("EN_gauges", k_en_gauges_w))
	{
		ImGui::TextDisabled("Organism  %d cells", stats.cell_count);
		ImGui::Separator();

		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		const ImVec2 available = ImGui::GetContentRegionAvail();

		// Bars fill whatever height is left after the trend row and the label block
		GaugeGeometry geo{};
		geo.column_w = std::floor(available.x / static_cast<float>(SlotCount));
		geo.trend_row_h = 16.f;
		geo.text_h = ImGui::GetTextLineHeight();
		geo.label_block_h = geo.text_h * 2.f + 6.f;
		geo.bar_h = std::max(available.y - geo.trend_row_h - geo.label_block_h, 10.f);
		geo.bar_w = std::min(geo.column_w - 10.f, 34.f);

		const GaugeView gauges[SlotCount] = {
			{ "NRG", tracker.total_energy,    tracker.max_energy,    stats.fill_fraction[SlotEnergy],
			  stats.repro_threshold_fraction[SlotEnergy],    stats.slope_per_tick[SlotEnergy],
			  fraction_color(stats.fill_fraction[SlotEnergy]) },
			{ "NUT", tracker.total_nutrients, tracker.max_nutrients, stats.fill_fraction[SlotNutrients],
			  stats.repro_threshold_fraction[SlotNutrients], stats.slope_per_tick[SlotNutrients],
			  k_en_col_nutrients },
			{ "INT", stats.total_integrity,   stats.max_integrity,   stats.fill_fraction[SlotIntegrity],
			  stats.repro_threshold_fraction[SlotIntegrity], stats.slope_per_tick[SlotIntegrity],
			  k_en_col_integrity },
		};

		for (int slot = 0; slot < SlotCount; ++slot)
		{
			const float column_x = origin.x + geo.column_w * static_cast<float>(slot);
			draw_gauge_column(draw_list, slot, gauges[slot], geo, column_x, origin.y);
		}
	}
	ImGui::EndChild();
}