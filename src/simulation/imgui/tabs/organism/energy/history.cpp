// Energy tab: History card (NRG / NUT / INT as % of organism max, min-max band of per-cell energy).
#include "energy_ui_common.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <iterator>

using namespace en_ui;

namespace
{
	// ── Series toggles ───────────────────────────────────────────────────────
	// The four checkboxes above the plot. Indices match OrganismTab::m_show_series_.
	enum SeriesToggle { ToggleEnergy = 0, ToggleNutrients = 1, ToggleIntegrity = 2, ToggleCells = 3, ToggleCount = 4 };

	const char* const k_toggle_names[ToggleCount] = { "NRG", "NUT", "INT", "Cells" };
	constexpr ImVec4  k_toggle_colors[ToggleCount] = {
		k_en_col_energy_line, k_en_col_nutrients, k_en_col_integrity, k_en_col_cell_band };

	// ── Visible slice of the recorded history ────────────────────────────────
	// A flat copy of the samples inside the selected time window, oldest first, laid out
	// the way ImPlot wants it (one x array, one y array per series).
	struct VisibleHistory
	{
		float ticks[EnergyHistory::capacity];
		float values[EnergyHistory::SeriesCount][EnergyHistory::capacity];
		int   count = 0;
	};

	void gather_visible_history(const EnergyHistory& history, const float window_ticks, VisibleHistory& out)
	{
		out.count = 0;
		if (history.empty())
			return;

		const int   total = history.size();
		const float oldest_visible_tick = history.newest().tick - window_ticks;

		// Walk back from the newest sample until we leave the window
		int first = total - 1;
		while (first > 0 && history.at(first - 1).tick >= oldest_visible_tick)
			--first;

		for (int i = first; i < total; ++i, ++out.count)
		{
			const EnergyHistory::Sample& sample = history.at(i);
			out.ticks[out.count] = sample.tick;
			for (int series = 0; series < EnergyHistory::SeriesCount; ++series)
				out.values[series][out.count] = sample.v[series];
		}
	}

	// ── Plot x range ─────────────────────────────────────────────────────────
	struct XRange { double lo, hi; };

	// The window ends at the newest sample. With no data yet, show an empty 20-tick span,
	// and never let the range collapse below 20 ticks.
	XRange compute_x_range(const VisibleHistory& data, const float window_ticks)
	{
		constexpr double min_span = 20.0;

		const double newest = data.count > 0 ? static_cast<double>(data.ticks[data.count - 1]) : min_span;
		const double lo = data.count > 0
			? std::max(static_cast<double>(data.ticks[0]), newest - window_ticks)
			: 0.0;
		return { lo, std::max(newest, lo + min_span) };
	}

	// ── Plot styling ─────────────────────────────────────────────────────────
	// Transparent frame / background / border so the card's own background shows through.
	// Pushes in the constructor, pops in the destructor.
	struct HistoryPlotStyleScope
	{
		HistoryPlotStyleScope()
		{
			ImPlot::PushStyleColor(ImPlotCol_FrameBg, { 0.f, 0.f, 0.f, 0.f });
			ImPlot::PushStyleColor(ImPlotCol_PlotBg, { 0.f, 0.f, 0.f, 0.f });
			ImPlot::PushStyleColor(ImPlotCol_PlotBorder, { 0.f, 0.f, 0.f, 0.f });
			ImPlot::PushStyleColor(ImPlotCol_AxisGrid, { 1.f, 1.f, 1.f, 0.07f });
			ImPlot::PushStyleColor(ImPlotCol_AxisText, { 0.55f, 0.58f, 0.65f, 1.f });
			ImPlot::PushStyleVar(ImPlotStyleVar_PlotBorderSize, 0.f);
		}
		~HistoryPlotStyleScope()
		{
			ImPlot::PopStyleVar(1);
			ImPlot::PopStyleColor(5);
		}
	};

	// ── Hover readout ────────────────────────────────────────────────────────
	// Snaps to the sample nearest the mouse, draws a vertical guide line, and shows every series' value.
	// Must be called inside an active ImPlot::BeginPlot / EndPlot pair.
	void draw_hover_readout(const VisibleHistory& data)
	{
		const double mouse_x = ImPlot::GetPlotMousePos().x;

		int    nearest = 0;
		double nearest_distance = DBL_MAX;
		for (int i = 0; i < data.count; ++i)
		{
			const double distance = std::abs(static_cast<double>(data.ticks[i]) - mouse_x);
			if (distance < nearest_distance)
			{
				nearest_distance = distance;
				nearest = i;
			}
		}

		const double guide_x = data.ticks[nearest];
		ImPlot::SetNextLineStyle({ 1.f, 1.f, 1.f, 0.40f }, 1.f);
		ImPlot::PlotInfLines("##hover", &guide_x, 1);

		const auto percent = [&](const int series) { return data.values[series][nearest] * 100.f; };

		ImGui::BeginTooltip();
		ImGui::TextDisabled("%.0f fr ago", static_cast<double>(data.ticks[data.count - 1]) - guide_x);
		ImGui::TextColored(k_en_col_energy_line, "NRG %.0f%%", percent(EnergyHistory::Energy));
		ImGui::TextColored(k_en_col_nutrients, "NUT %.0f%%", percent(EnergyHistory::Nutrients));
		ImGui::TextColored(k_en_col_integrity, "INT %.0f%%", percent(EnergyHistory::Integrity));
		ImGui::TextColored(k_en_col_cell_band, "Cells %.0f-%.0f%%  (avg %.0f%%)",
			percent(EnergyHistory::CellMin), percent(EnergyHistory::CellMax), percent(EnergyHistory::CellMean));
		ImGui::EndTooltip();
	}

	// ── The plot ─────────────────────────────────────────────────────────────
	// All series are fractions of organism capacity, so the y axis is fixed at 0..100%.
	void draw_history_plot(const VisibleHistory& data, const XRange& x_range,
		const float energy_repro_threshold, const bool(&show)[ToggleCount])
	{
		const HistoryPlotStyleScope plot_style;

		constexpr ImPlotFlags plot_flags = ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect |
			ImPlotFlags_NoMouseText | ImPlotFlags_NoLegend | ImPlotFlags_NoInputs;

		if (!ImPlot::BeginPlot("##en_hist", { -1.f, -1.f }, plot_flags))
			return;

		static const double y_tick_positions[] = { 0.0, 0.25, 0.5, 0.75, 1.0 };
		static const char* const y_tick_labels[] = { "0", "25%", "50%", "75%", "100%" };
		ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoTickMarks, ImPlotAxisFlags_NoTickMarks);
		ImPlot::SetupAxisLimits(ImAxis_X1, x_range.lo, x_range.hi, ImGuiCond_Always);
		ImPlot::SetupAxisLimits(ImAxis_Y1, -0.03, 1.05, ImGuiCond_Always);
		ImPlot::SetupAxisTicks(ImAxis_Y1, y_tick_positions, 5, y_tick_labels);

		// Faint horizontal line at the energy reproduction threshold
		if (energy_repro_threshold > 0.f)
		{
			const double line_x[2] = { x_range.lo, x_range.hi };
			const double line_y[2] = { energy_repro_threshold, energy_repro_threshold };
			ImPlot::SetNextLineStyle({ 1.f, 1.f, 1.f, 0.30f }, 1.f);
			ImPlot::PlotLine("##thr", line_x, line_y, 2);
		}

		// Lines need at least two points. Drawn back to front so energy ends up on top.
		if (data.count > 1)
		{
			if (show[ToggleCells])
			{
				ImPlot::SetNextFillStyle(k_en_col_cell_band, 0.15f);
				ImPlot::PlotShaded("##band", data.ticks, data.values[EnergyHistory::CellMin],
					data.values[EnergyHistory::CellMax], data.count);
				ImPlot::SetNextLineStyle({ k_en_col_cell_band.x, k_en_col_cell_band.y, k_en_col_cell_band.z, 0.45f }, 1.f);
				ImPlot::PlotLine("##cmean", data.ticks, data.values[EnergyHistory::CellMean], data.count);
			}
			if (show[ToggleIntegrity])
			{
				ImPlot::SetNextLineStyle(k_en_col_integrity, 1.5f);
				ImPlot::PlotLine("##i", data.ticks, data.values[EnergyHistory::Integrity], data.count);
			}
			if (show[ToggleNutrients])
			{
				ImPlot::SetNextLineStyle(k_en_col_nutrients, 1.5f);
				ImPlot::PlotLine("##n", data.ticks, data.values[EnergyHistory::Nutrients], data.count);
			}
			if (show[ToggleEnergy])
			{
				ImPlot::SetNextLineStyle(k_en_col_energy_line, 2.f);
				ImPlot::PlotLine("##e", data.ticks, data.values[EnergyHistory::Energy], data.count);
			}
		}

		if (data.count > 0 && ImPlot::IsPlotHovered())
			draw_hover_readout(data);

		ImPlot::EndPlot();
	}

	// ── History window selector ──────────────────────────────────────────────
	// Combo box choosing how many frames of history to show; `selected` indexes k_energy_windows.
	void draw_window_combo(int& selected)
	{
		const int window_count = static_cast<int>(std::size(k_energy_windows));

		ImGui::SetNextItemWidth(76.f);
		char current_label[16];
		std::snprintf(current_label, sizeof current_label, "%d fr", k_energy_windows[selected]);

		if (ImGui::BeginCombo("##win", current_label))
		{
			for (int i = 0; i < window_count; ++i)
			{
				char label[16];
				std::snprintf(label, sizeof label, "%d fr", k_energy_windows[i]);
				if (ImGui::Selectable(label, i == selected))
					selected = i;
			}
			ImGui::EndCombo();
		}
		ImGui::SetItemTooltip("History window");
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  History card: NRG / NUT / INT as % of organism max, min-max band of per-cell energy
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_energy_history(const EnergyStats& stats, const float card_width)
{
	if (begin_card("EN_hist", card_width))
	{
		// Series toggles, each tinted with its series colour
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 2.f, 1.f });
		for (int i = 0; i < ToggleCount; ++i)
		{
			if (i > 0)
				ImGui::SameLine();
			ImGui::PushID(i);
			ImGui::PushStyleColor(ImGuiCol_CheckMark, k_toggle_colors[i]);
			ImGui::PushStyleColor(ImGuiCol_Text, k_toggle_colors[i]);
			ImGui::Checkbox(k_toggle_names[i], &m_show_series_[i]);
			ImGui::PopStyleColor(2);
			ImGui::PopID();
		}
		ImGui::PopStyleVar();
		ImGui::SetItemTooltip("Toggle series. 'Cells' = min-max band of per-cell energy");

		ImGui::SameLine();
		draw_window_combo(m_energy_window_);

		// Copy the samples inside the selected window into plot-ready arrays.
		// Static because the buffers are large (history capacity x series count).
		static VisibleHistory visible;
		const float window_ticks = static_cast<float>(k_energy_windows[m_energy_window_]);
		gather_visible_history(m_energy_history_, window_ticks, visible);

		const bool show[ToggleCount] = {
			m_show_series_[ToggleEnergy], m_show_series_[ToggleNutrients],
			m_show_series_[ToggleIntegrity], m_show_series_[ToggleCells] };

		draw_history_plot(visible, compute_x_range(visible, window_ticks),
			stats.repro_threshold_fraction[static_cast<int>(EnergyHistory::Energy)], show);
	}
	ImGui::EndChild();
}