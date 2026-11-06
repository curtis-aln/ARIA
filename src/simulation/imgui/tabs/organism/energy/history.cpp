// Energy tab — History card (NRG / NUT / INT as % of organism max, min–max per-cell energy band).
#include "energy_ui_common.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>

using namespace en_ui;

// ─────────────────────────────────────────────────────────────────────────────
//  History — NRG / NUT / INT as % of organism max, min–max per-cell energy band
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_energy_history(const EnergyStats& st, float w)
{
	if (begin_card("EN_hist", w))
	{
		static const char* names[4] = { "NRG", "NUT", "INT", "Cells" };
		static constexpr ImVec4 cols[4] = { k_en_col_energy_line, k_en_col_nutrients, k_en_col_integrity, k_en_col_cell_band };

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 2.f, 1.f });
		for (int i = 0; i < 4; ++i)
		{
			if (i) ImGui::SameLine();
			ImGui::PushID(i);
			ImGui::PushStyleColor(ImGuiCol_CheckMark, cols[i]);
			ImGui::PushStyleColor(ImGuiCol_Text, cols[i]);
			ImGui::Checkbox(names[i], &m_show_series_[i]);
			ImGui::PopStyleColor(2);
			ImGui::PopID();
		}
		ImGui::PopStyleVar();
		ImGui::SetItemTooltip("Toggle series. 'Cells' = min-max band of per-cell energy");

		ImGui::SameLine();
		ImGui::SetNextItemWidth(76.f);
		char cur[16];
		std::snprintf(cur, sizeof cur, "%d fr", k_energy_windows[m_energy_window_]);
		if (ImGui::BeginCombo("##win", cur))
		{
			for (int j = 0; j < 3; ++j)
			{
				char lbl[16];
				std::snprintf(lbl, sizeof lbl, "%d fr", k_energy_windows[j]);
				if (ImGui::Selectable(lbl, j == m_energy_window_))
					m_energy_window_ = j;
			}
			ImGui::EndCombo();
		}
		ImGui::SetItemTooltip("History window");

		// ── Gather the visible slice, oldest first ─────────────────────────
		const EnergyHistory& h = m_energy_hist_;
		const float window = static_cast<float>(k_energy_windows[m_energy_window_]);
		static float xs[EnergyHistory::capacity];
		static float ys[EnergyHistory::SeriesCount][EnergyHistory::capacity];
		int m = 0;
		if (!h.empty())
		{
			const int cnt = h.size();
			const float x_cut = h.newest().tick - window;
			int first = cnt - 1;
			while (first > 0 && h.at(first - 1).tick >= x_cut) --first;
			for (int k = first; k < cnt; ++k, ++m)
			{
				const EnergyHistory::Sample& s = h.at(k);
				xs[m] = s.tick;
				for (int j = 0; j < EnergyHistory::SeriesCount; ++j)
					ys[j][m] = s.v[j];
			}
		}

		const double x_hi0 = m > 0 ? static_cast<double>(xs[m - 1]) : 20.0;
		const double x_lo = m > 0 ? std::max(static_cast<double>(xs[0]), x_hi0 - window) : 0.0;
		const double x_hi = std::max(x_hi0, x_lo + 20.0);

		ImPlot::PushStyleColor(ImPlotCol_FrameBg, { 0.f, 0.f, 0.f, 0.f });
		ImPlot::PushStyleColor(ImPlotCol_PlotBg, { 0.f, 0.f, 0.f, 0.f });
		ImPlot::PushStyleColor(ImPlotCol_PlotBorder, { 0.f, 0.f, 0.f, 0.f });
		ImPlot::PushStyleColor(ImPlotCol_AxisGrid, { 1.f, 1.f, 1.f, 0.07f });
		ImPlot::PushStyleColor(ImPlotCol_AxisText, { 0.55f, 0.58f, 0.65f, 1.f });
		ImPlot::PushStyleVar(ImPlotStyleVar_PlotBorderSize, 0.f);

		constexpr ImPlotFlags pf = ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect |
			ImPlotFlags_NoMouseText | ImPlotFlags_NoLegend | ImPlotFlags_NoInputs;

		if (ImPlot::BeginPlot("##en_hist", { -1.f, -1.f }, pf))
		{
			static const double y_ticks[] = { 0.0, 0.25, 0.5, 0.75, 1.0 };
			static const char* const y_labels[] = { "0", "25%", "50%", "75%", "100%" };
			ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoTickMarks, ImPlotAxisFlags_NoTickMarks);
			ImPlot::SetupAxisLimits(ImAxis_X1, x_lo, x_hi, ImGuiCond_Always);
			ImPlot::SetupAxisLimits(ImAxis_Y1, -0.03, 1.05, ImGuiCond_Always);
			ImPlot::SetupAxisTicks(ImAxis_Y1, y_ticks, 5, y_labels);

			// Reproduction energy threshold
			if (st.thr[0] > 0.f)
			{
				const double bx[2] = { x_lo, x_hi }, by[2] = { st.thr[0], st.thr[0] };
				ImPlot::SetNextLineStyle({ 1.f, 1.f, 1.f, 0.30f }, 1.f);
				ImPlot::PlotLine("##thr", bx, by, 2);
			}

			if (m > 1)
			{
				if (m_show_series_[3])
				{
					ImPlot::SetNextFillStyle(k_en_col_cell_band, 0.15f);
					ImPlot::PlotShaded("##band", xs, ys[EnergyHistory::CellMin], ys[EnergyHistory::CellMax], m);
					ImPlot::SetNextLineStyle({ k_en_col_cell_band.x, k_en_col_cell_band.y, k_en_col_cell_band.z, 0.45f }, 1.f);
					ImPlot::PlotLine("##cmean", xs, ys[EnergyHistory::CellMean], m);
				}
				if (m_show_series_[2])
				{
					ImPlot::SetNextLineStyle(k_en_col_integrity, 1.5f);
					ImPlot::PlotLine("##i", xs, ys[EnergyHistory::Integrity], m);
				}
				if (m_show_series_[1])
				{
					ImPlot::SetNextLineStyle(k_en_col_nutrients, 1.5f);
					ImPlot::PlotLine("##n", xs, ys[EnergyHistory::Nutrients], m);
				}
				if (m_show_series_[0])
				{
					ImPlot::SetNextLineStyle(k_en_col_energy_line, 2.f);
					ImPlot::PlotLine("##e", xs, ys[EnergyHistory::Energy], m);
				}
			}

			// Hover: nearest sample, vertical line + readout
			if (m > 0 && ImPlot::IsPlotHovered())
			{
				const double mx = ImPlot::GetPlotMousePos().x;
				int best = 0;
				double bd = DBL_MAX;
				for (int k = 0; k < m; ++k)
				{
					const double d = std::abs(static_cast<double>(xs[k]) - mx);
					if (d < bd) { bd = d; best = k; }
				}
				const double hx = xs[best];
				ImPlot::SetNextLineStyle({ 1.f, 1.f, 1.f, 0.40f }, 1.f);
				ImPlot::PlotInfLines("##hover", &hx, 1);

				ImGui::BeginTooltip();
				ImGui::TextDisabled("%.0f fr ago", static_cast<double>(xs[m - 1]) - hx);
				ImGui::TextColored(k_en_col_energy_line, "NRG %.0f%%", ys[EnergyHistory::Energy][best] * 100.f);
				ImGui::TextColored(k_en_col_nutrients, "NUT %.0f%%", ys[EnergyHistory::Nutrients][best] * 100.f);
				ImGui::TextColored(k_en_col_integrity, "INT %.0f%%", ys[EnergyHistory::Integrity][best] * 100.f);
				ImGui::TextColored(k_en_col_cell_band, "Cells %.0f-%.0f%%  (avg %.0f%%)",
					ys[EnergyHistory::CellMin][best] * 100.f, ys[EnergyHistory::CellMax][best] * 100.f,
					ys[EnergyHistory::CellMean][best] * 100.f);
				ImGui::EndTooltip();
			}
			ImPlot::EndPlot();
		}
		ImPlot::PopStyleVar(1);
		ImPlot::PopStyleColor(5);
	}
	ImGui::EndChild();
}