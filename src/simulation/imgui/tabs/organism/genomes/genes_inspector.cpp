// Genes tab: the right-hand inspector for the selected gene.
//   - distribution strip (every entity's value as a dot on the gene's limits)
//   - numbers for the selected entity
//   - live edit (this entity, or copy to all)
//   - wave preview for oscillator genes
#include "genes_ui_common.h"

#include <array>
#include <cstdio>
#include <implot.h>

namespace genes_ui
{
	namespace
	{
		constexpr ImU32 k_col_strip_bg = IM_COL32(22, 24, 30, 255);
		constexpr ImU32 k_col_strip_axis = IM_COL32(90, 96, 112, 255);
		constexpr ImU32 k_col_strip_spawn = IM_COL32(70, 130, 190, 55);
		constexpr ImU32 k_col_strip_mean = IM_COL32(255, 255, 255, 110);
		constexpr ImU32 k_col_strip_label = IM_COL32(120, 126, 145, 255);

		constexpr float k_strip_margin = 8.f;          // inset of the track inside the strip
		constexpr float k_strip_pick_radius = 12.f;    // how close the mouse must be to pick a dot
		constexpr float k_dot_radius = 4.f;

		// ── Live editing ─────────────────────────────────────────────────────
		// SimCommand fields: int_val = gene index, cell_tendon_idx = absolute cell id / tendon id, float_val = phenotype value
		template <class K>
		void push_gene_cmd(GnCtx& g, const uint32_t id, const size_t gene, const float value)
		{
			SimCommand cmd{ .section = CommandSection::CellManagerEvent, .type = K::command() };
			cmd.cell_tendon_idx = static_cast<int>(id);
			cmd.int_val = static_cast<int>(gene);
			cmd.float_val = value;
			g.ctx.push(cmd);
		}

		// ── Distribution strip ───────────────────────────────────────────────
		// Every entity's value for one gene as a dot on its limits, with the spawn band and the organism mean.
		// Click a dot to select that entity.
		template <class K>
		void draw_strip(GnCtx& g, const size_t gene, const int entity_total, const RowStats& stats)
		{
			const auto& order = K::order(g);
			const auto& first_genome = K::genome(g.t, order[0]);
			const Range spawn = first_genome.gene_initial_range(gene);
			const Range limits = first_genome.limits(gene);

			const float width = ImGui::GetContentRegionAvail().x;
			const ImVec2 p0 = ImGui::GetCursorScreenPos();
			ImGui::InvisibleButton("##gn_strip", { width, k_strip_h });
			const bool hovered = ImGui::IsItemHovered();
			const bool clicked = ImGui::IsItemClicked();
			ImDrawList* dl = ImGui::GetWindowDrawList();

			const float track_left = p0.x + k_strip_margin;
			const float track_right = p0.x + width - k_strip_margin;
			const float track_len = track_right - track_left;
			const float center_y = p0.y + k_strip_h * 0.5f;

			dl->AddRectFilled(p0, { p0.x + width, p0.y + k_strip_h }, k_col_strip_bg, 3.f);

			// Generation-0 spawn range as a shaded band (at least 2px wide so a point range stays visible)
			const float band_left = track_left + spawn.min * track_len;
			const float band_right = std::max(track_left + spawn.max * track_len, band_left + 2.f);
			dl->AddRectFilled({ band_left, p0.y + 3.f }, { band_right, p0.y + k_strip_h - 3.f }, k_col_strip_spawn);

			// Track with end ticks
			dl->AddLine({ track_left, center_y }, { track_right, center_y }, k_col_strip_axis, 1.f);
			dl->AddLine({ track_left, center_y - 5.f }, { track_left, center_y + 5.f }, k_col_strip_axis, 1.f);
			dl->AddLine({ track_right, center_y - 5.f }, { track_right, center_y + 5.f }, k_col_strip_axis, 1.f);

			// Organism mean
			const float mean_x = track_left + stats.mean_norm * track_len;
			dl->AddLine({ mean_x, p0.y + 3.f }, { mean_x, p0.y + k_strip_h - 3.f }, k_col_strip_mean, 1.f);

			// One dot per entity; dots that would overlap are nudged onto three lanes
			std::array<float, k_max_entities> dot_x{};
			const float mouse_x = ImGui::GetIO().MousePos.x;
			int nearest = -1;
			float nearest_distance = 1e9f;

			for (int k = 0; k < entity_total; ++k)
			{
				const int idx = order[k];
				const Sample s = read<K>(g.t, idx, gene);
				const float x = track_left + s.norm * track_len;
				dot_x[k] = x;

				int lane = 0;
				for (int j = 0; j < k; ++j)
					if (std::abs(dot_x[j] - x) < 7.f)
						++lane;
				const float y = center_y + (static_cast<float>(lane % 3) - 1.f) * 9.f;

				dl->AddCircleFilled({ x, y }, k_dot_radius, bright32(K::outer(g.t, idx), 255));
				dl->AddCircle({ x, y }, k_dot_radius, IM_COL32(0, 0, 0, 160));
				if (idx == K::target(g))
					dl->AddCircle({ x, y }, 6.5f, IM_COL32(255, 255, 255, 230), 0, 1.5f);

				const float distance = std::abs(mouse_x - x);
				if (distance < nearest_distance)
				{
					nearest_distance = distance;
					nearest = k;
				}
			}

			if (hovered && nearest >= 0 && nearest_distance < k_strip_pick_radius)
			{
				const Sample s = read<K>(g.t, order[nearest], gene);
				ImGui::SetTooltip("%s %d : %.5f", K::noun, nearest, s.pheno);
				if (clicked)
					K::select(g, order[nearest]);
			}

			// Limit labels under the track ends
			char low[24], high[24];
			std::snprintf(low, sizeof low, "%.4g", limits.min);
			std::snprintf(high, sizeof high, "%.4g", limits.max);
			const float label_y = p0.y + k_strip_h + 1.f;
			dl->AddText({ track_left, label_y }, k_col_strip_label, low);
			dl->AddText({ track_right - ImGui::CalcTextSize(high).x, label_y }, k_col_strip_label, high);
			ImGui::Dummy({ width, ImGui::GetTextLineHeight() + 2.f });
		}

		// ── Wave preview ─────────────────────────────────────────────────────
		// Transparent plot chrome so the card background shows through. Pushes in the constructor, pops in the destructor.
		struct WavePlotStyleScope
		{
			WavePlotStyleScope()
			{
				ImPlot::PushStyleColor(ImPlotCol_FrameBg, { 0.f, 0.f, 0.f, 0.f });
				ImPlot::PushStyleColor(ImPlotCol_PlotBg, { 0.f, 0.f, 0.f, 0.f });
				ImPlot::PushStyleColor(ImPlotCol_PlotBorder, { 0.f, 0.f, 0.f, 0.f });
				ImPlot::PushStyleColor(ImPlotCol_AxisGrid, { 1.f, 1.f, 1.f, 0.07f });
				ImPlot::PushStyleColor(ImPlotCol_AxisText, { 0.55f, 0.58f, 0.65f, 1.f });
				ImPlot::PushStyleVar(ImPlotStyleVar_PlotBorderSize, 0.f);
			}
			~WavePlotStyleScope()
			{
				ImPlot::PopStyleVar(1);
				ImPlot::PopStyleColor(5);
			}
		};

		// Every entity's wave for the next N frames, driven by the live phase so tendon sync shows up.
		// The selected entity is drawn last (thicker, opaque) so it sits on top.
		template <class K>
		void draw_wave_overlay(GnCtx& g, const int entity_total)
		{
			GenesTabState& st = g.st;
			const auto& order = K::order(g);

			static double xs[k_wave_samples], ys[k_wave_samples];
			const double window = static_cast<double>(st.wave_window);
			for (int i = 0; i < k_wave_samples; ++i)
				xs[i] = window * static_cast<double>(i) / static_cast<double>(k_wave_samples - 1);

			{
				const WavePlotStyleScope plot_style;
				constexpr ImPlotFlags plot_flags = ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect |
					ImPlotFlags_NoMouseText | ImPlotFlags_NoLegend | ImPlotFlags_NoInputs;

				if (ImPlot::BeginPlot("##gn_wave", { -1.f, 70.f }, plot_flags))
				{
					static const double y_ticks[] = { 0.0, 0.5, 1.0 };
					ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoTickMarks, ImPlotAxisFlags_NoTickMarks);
					ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, window, ImGuiCond_Always);
					ImPlot::SetupAxisLimits(ImAxis_Y1, -0.06, 1.06, ImGuiCond_Always);
					ImPlot::SetupAxisTicks(ImAxis_Y1, y_ticks, 3);

					for (int pass = 0; pass < 2; ++pass)   // pass 0: everyone else, pass 1: the selected entity
					{
						for (int k = 0; k < entity_total; ++k)
						{
							const int idx = order[k];
							const bool is_selected = idx == K::target(g);
							if (is_selected != (pass == 1))
								continue;

							const auto& genome = K::genome(g.t, idx);
							const float phase = K::phase(g.t, idx);
							for (int i = 0; i < k_wave_samples; ++i)
								ys[i] = static_cast<double>(K::wave_level(genome, phase, static_cast<float>(xs[i])));

							char series_id[16];
							std::snprintf(series_id, sizeof series_id, "##w%d", k);
							ImPlot::SetNextLineStyle(bright4(K::outer(g.t, idx), is_selected ? 1.f : 0.55f), is_selected ? 2.5f : 1.2f);
							ImPlot::PlotLine(series_id, xs, ys, k_wave_samples);
						}
					}
					ImPlot::EndPlot();
				}
			}

			ImGui::SetNextItemWidth(-1.f);
			ImGui::SliderInt("##gn_win", &st.wave_window, 60, 600, "Next %d frames (live phase)");
		}

		// ── Inspector sections ───────────────────────────────────────────────
		// Chip + gene name, then which group / entity this is
		template <class K>
		void draw_inspector_title(const GnCtx& g, const GeneInfo& info, const int target, const int rank)
		{
			draw_inline_chip(K::outer(g.t, target), K::inner(g.t, target));
			ImGui::SameLine(0.f, 4.f);
			ImGui::TextColored(k_accent, "%s", info.name);
			ImGui::TextDisabled("%s  |  %s %d", gene_group_name(info.group), K::noun, rank);
		}

		// Numbers for the selected entity, plus how it compares with the organism
		template <class K>
		void draw_selected_numbers(const GnCtx& g, const GeneInfo& info, const int target, const size_t gene,
			const int entity_total, const Sample& s, const RowStats& stats)
		{
			const auto& genome = K::genome(g.t, target);
			const Range limits = genome.limits(gene);
			const Range spawn = genome.gene_initial_range(gene);
			const float span = limits.max - limits.min;

			draw_property("Value", "%.5f", s.pheno);

			char derived[64];
			if (derived_text(info, s.pheno, K::maxes(g.t, target), derived, sizeof derived))
				draw_property_colored("", k_derived, "%s", derived);

			draw_property("Gene", "%.3f", s.norm);
			draw_property("Sigma", "%.4f", s.sigma);
			draw_property("Limits", "%.4g .. %.4g", limits.min, limits.max);
			draw_property("Spawn", "%.4g .. %.4g", limits.min + spawn.min * span, limits.min + spawn.max * span);
			if (entity_total >= 2)
				draw_property("Organism", "mean %.4g   spread %.2f", stats.mean_pheno, stats.spread);

			draw_flag_lines(flags_for<K>(g.t, target, gene, s.norm));
		}

		// Slider that edits this entity only, and a button that copies the value to every entity
		template <class K>
		void draw_edit_controls(GnCtx& g, const int target, const size_t gene, const int entity_total, const Sample& s)
		{
			const OrganismTracker& t = g.t;
			const auto& order = K::order(g);
			const Range limits = K::genome(t, target).limits(gene);

			float value = s.pheno;
			ImGui::SetNextItemWidth(-64.f);
			if (ImGui::SliderFloat("##gn_edit", &value, limits.min, limits.max, "%.4f"))
				push_gene_cmd<K>(g, K::id(t, target), gene, value);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Edits this %s only. Ctrl+click to type a value.", K::noun);

			ImGui::SameLine();
			if (ImGui::Button("Set all"))
			{
				for (int k = 0; k < entity_total; ++k)
					push_gene_cmd<K>(g, K::id(t, order[k]), gene, s.pheno);
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Copy this value to every %s of the organism", K::plural);
		}
	}

	// ─────────────────────────────────────────────────────────────────────────
	//  Inspector body
	// ─────────────────────────────────────────────────────────────────────────
	template <class K>
	void draw_inspector_body(GnCtx& g)
	{
		GenesTabState& st = g.st;
		const OrganismTracker& t = g.t;
		const auto& infos = K::info();
		const int entity_total = entity_count<K>(g);
		const size_t gene = static_cast<size_t>(st.sel_gene);

		if (entity_total <= 0 || gene >= infos.size())
		{
			ImGui::TextDisabled("No %s in this organism.", K::plural);
			return;
		}

		const GeneInfo& info = infos[gene];
		const int target = std::clamp(K::target(g), 0, K::count(t) - 1);
		const int rank = K::rank(g)[static_cast<size_t>(target)];
		const RowStats stats = compute_stats<K>(g, gene, entity_total);
		const Sample sample = read<K>(t, target, gene);

		draw_inspector_title<K>(g, info, target, rank);
		ImGui::Spacing();
		draw_strip<K>(g, gene, entity_total, stats);
		ImGui::TextDisabled("shaded = gen-0 spawn range    line = organism mean");

		draw_section_title("Selected");
		draw_selected_numbers<K>(g, info, target, gene, entity_total, sample, stats);

		if (info.editable)
		{
			draw_section_title("Edit");
			draw_edit_controls<K>(g, target, gene, entity_total, sample);
		}

		if (info.group == K::wave_group)
		{
			draw_section_title("Wave preview");
			draw_wave_overlay<K>(g, entity_total);
		}

		draw_section_title("About");
		ImGui::PushTextWrapPos(0.f);
		ImGui::TextDisabled("%s", info.tip);
		ImGui::PopTextWrapPos();
	}

	template void draw_inspector_body<CellKind>(GnCtx&);
	template void draw_inspector_body<TendonKind>(GnCtx&);
}