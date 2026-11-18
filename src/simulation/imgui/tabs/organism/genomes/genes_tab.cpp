// Genes tab: top-level layout, toolbar, and the summary chips.
//
//  Left:  two matrices side by side. Genes run down the rows, entities across the columns.
//         CELLS shows every cell's CellGenome, SPRINGS shows every tendon's own TendonGenome.
//  Right: inspector for the selected gene (distribution strip, numbers, live edit, wave preview).
//
//  Everything is read straight from the snapshot each frame; nothing is cached between frames
//  except the view settings in GenesTabState (organism_tab.h).
#include "genes_ui_common.h"

#include <cmath>
#include <cstdint>
#include <cstdio>

namespace genes_ui
{
	namespace
	{
		constexpr ImU32 k_col_chip_bg = IM_COL32(30, 33, 42, 255);
		constexpr ImU32 k_col_chip_border = IM_COL32(58, 62, 78, 255);
		constexpr ImVec4 k_chip_label = { 0.55f, 0.58f, 0.65f, 1.f };

		// ── Summary chips ────────────────────────────────────────────────────
		// Numbers gathered from the tracker for the chip row
		struct OrganismSummary
		{
			int      cells = 0;
			int      springs = 0;
			uint32_t generation_min = 0;
			uint32_t generation_max = 0;
			float    mean_sigma_cells = 0.f;
			float    mean_sigma_springs = 0.f;
			int      quorum_needed = 0;   // cells that must be ready for the organism to reproduce
			int      cells_ready = 0;     // cells that are ready right now
		};

		OrganismSummary summarise_organism(const OrganismTracker& t)
		{
			OrganismSummary summary;
			summary.cells = CellKind::count(t);
			summary.springs = TendonKind::count(t);
			if (summary.cells == 0)
				return summary;

			summary.generation_min = UINT32_MAX;
			float quorum_sum = 0.f, sigma_cells = 0.f, sigma_springs = 0.f;
			for (int i = 0; i < summary.cells; ++i)
			{
				const Cell& cell = t.cells[i];
				summary.generation_min = std::min(summary.generation_min, cell.generation());
				summary.generation_max = std::max(summary.generation_max, cell.generation());
				quorum_sum += cell.get<CellGenome::QuorumFraction>();
				summary.cells_ready += cell.should_reproduce() ? 1 : 0;
				sigma_cells += cell.mean_mutation_strength(CellGenome::SharedCount, CellGenome::gene_count);
			}
			for (int i = 0; i < summary.springs; ++i)
				sigma_springs += t.springs[i].genome.mean_mutation_strength(TendonGenome::SharedCount, TendonGenome::gene_count);

			summary.mean_sigma_cells = sigma_cells / static_cast<float>(summary.cells);
			summary.mean_sigma_springs = summary.springs > 0 ? sigma_springs / static_cast<float>(summary.springs) : 0.f;

			// same rule as CellManager::can_protozoa_reproduce
			const float quorum = quorum_sum / static_cast<float>(summary.cells);
			summary.quorum_needed = std::max(1, static_cast<int>(std::ceil(quorum * static_cast<float>(summary.cells))));
			return summary;
		}

		// Chips flow left to right and wrap onto a new line when the next one doesn't fit
		struct ChipFlow { bool first = true; };

		// Rounded "label value" chip. `tooltip` may be null.
		void draw_stat_chip(ChipFlow& flow, const char* label, const char* value, const ImVec4& value_color,
			const char* tooltip = nullptr)
		{
			constexpr float pad_x = 6.f, gap = 5.f, rounding = 4.f;

			const ImVec2 label_size = ImGui::CalcTextSize(label);
			const ImVec2 value_size = ImGui::CalcTextSize(value);
			const float height = ImGui::GetTextLineHeight() + 4.f;
			const float width = pad_x * 2.f + label_size.x + gap + value_size.x;

			if (!flow.first)
			{
				const float spacing = ImGui::GetStyle().ItemSpacing.x;
				const float right_edge = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
				if (ImGui::GetItemRectMax().x + spacing + width <= right_edge)
					ImGui::SameLine(0.f, spacing);
			}
			flow.first = false;

			const ImVec2 p0 = ImGui::GetCursorScreenPos();
			ImDrawList* dl = ImGui::GetWindowDrawList();
			dl->AddRectFilled(p0, { p0.x + width, p0.y + height }, k_col_chip_bg, rounding);
			dl->AddRect(p0, { p0.x + width, p0.y + height }, k_col_chip_border, rounding);
			dl->AddText({ p0.x + pad_x, p0.y + 2.f }, ImGui::GetColorU32(k_chip_label), label);
			dl->AddText({ p0.x + pad_x + label_size.x + gap, p0.y + 2.f }, ImGui::GetColorU32(value_color), value);

			ImGui::Dummy({ width, height });
			if (tooltip && ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", tooltip);
		}

		void draw_summary(const GnCtx& g)
		{
			const OrganismSummary s = summarise_organism(g.t);
			if (s.cells == 0)
			{
				ImGui::TextDisabled("No cells");
				return;
			}

			ChipFlow flow;
			char value[32];

			std::snprintf(value, sizeof value, "%d", s.cells);
			draw_stat_chip(flow, "Cells", value, k_value_text);

			std::snprintf(value, sizeof value, "%d", s.springs);
			draw_stat_chip(flow, "Springs", value, k_value_text);

			if (s.generation_min == s.generation_max)
				std::snprintf(value, sizeof value, "%u", s.generation_max);
			else
				std::snprintf(value, sizeof value, "%u-%u", s.generation_min, s.generation_max);
			draw_stat_chip(flow, "Gen", value, k_value_text);

			std::snprintf(value, sizeof value, "%.4f", s.mean_sigma_cells);
			draw_stat_chip(flow, "Sigma cells", value, k_value_text, "Mean mutation strength across cell genes");

			std::snprintf(value, sizeof value, "%.4f", s.mean_sigma_springs);
			draw_stat_chip(flow, "Sigma springs", value, k_value_text, "Mean mutation strength across spring genes");

			// Green once enough cells are ready to reproduce
			char tooltip[96];
			std::snprintf(tooltip, sizeof tooltip, "%d of %d cells must be ready for the organism to reproduce (%d now)",
				s.quorum_needed, s.cells, s.cells_ready);
			std::snprintf(value, sizeof value, "%d / %d", s.cells_ready, s.quorum_needed);
			draw_stat_chip(flow, "Ready", value, s.cells_ready >= s.quorum_needed ? k_good : k_orange, tooltip);
		}

		// ── Toolbar ──────────────────────────────────────────────────────────
		// Legend for the matrix, shown when hovering the "(?)" in the toolbar
		void draw_help_tooltip()
		{
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.f);

			draw_property("Value", "real units");
			draw_property("Gene", "raw 0..1 gene value");
			draw_property("Sigma", "per-gene mutation strength");

			ImGui::Spacing();
			ImGui::TextWrapped("Bar = position inside the gene's limits (Sigma view: log position inside the sigma range).");
			ImGui::TextWrapped("Spread = how much the organism's cells differ for this trait (0 = identical).");
			ImGui::TextColored(k_orange, "Orange edge = gene sits at one of its limits");
			ImGui::TextColored(k_accent, "Small triangle = outside the generation-0 spawn range (up = above, down = below)");
			ImGui::TextDisabled("Clicking a cell or header selects it here and in Cells & Springs");

			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}

		void draw_toolbar(GenesTabState& st)
		{
			ImGui::AlignTextToFramePadding();
			ImGui::TextDisabled("Show");
			ImGui::SameLine();

			static const char* const metric_labels[] = { "Value", "Gene", "Sigma" };
			int metric = static_cast<int>(st.metric);
			if (segmented_control("##gn_metric", metric_labels, 3, metric))
				st.metric = static_cast<GnMetric>(metric);

			ImGui::SameLine(0.f, 14.f);
			ImGui::Checkbox("Sort by spread", &st.sort_by_spread);
			ImGui::SameLine();
			ImGui::Checkbox("Hide converged", &st.hide_converged);
			ImGui::SameLine();
			ImGui::Checkbox("Flags", &st.show_flags);

			// Gene name filter with a clear button that only appears while there is text
			ImGui::SameLine();
			ImGui::SetNextItemWidth(110.f);
			ImGui::InputTextWithHint("##gn_filter", "filter genes", st.filter, sizeof st.filter);
			if (st.filter[0] != '\0')
			{
				ImGui::SameLine(0.f, 2.f);
				if (ImGui::SmallButton("x##gn_filter_clear"))
					st.filter[0] = '\0';
			}

			ImGui::SameLine();
			ImGui::TextDisabled("(?)");
			if (ImGui::IsItemHovered())
				draw_help_tooltip();
		}

		// ── Layout ───────────────────────────────────────────────────────────
		struct TableWidths { float cells, springs; };

		// Width a matrix wants so none of its columns need to scroll
		float wanted_table_width(const int entity_total)
		{
			return entity_total > 0
				? k_name_w + static_cast<float>(entity_total) * k_col_w + k_mean_w + k_spread_w + k_table_extra
				: 200.f;
		}

		// Spare room is shared out in proportion to what each table wants. When there isn't enough room
		// the split is still proportional, but neither table gets narrower than k_min_table_w and they scroll sideways.
		TableWidths split_table_widths(const int cell_columns, const int spring_columns, const float total_w)
		{
			const float want_cells = wanted_table_width(cell_columns);
			const float want_springs = wanted_table_width(spring_columns);

			float cells_w;
			if (want_cells + want_springs <= total_w)
				cells_w = want_cells + (total_w - want_cells - want_springs) * (want_cells / (want_cells + want_springs));
			else
				cells_w = std::clamp(total_w * want_cells / (want_cells + want_springs), k_min_table_w,
					std::max(k_min_table_w, total_w - k_min_table_w));
			return { cells_w, total_w - cells_w };
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Top-level draw
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_genes_tab(const SimSnapshot& /*snap*/, ImGuiContext& ctx, const OrganismTracker& protozoa)
{
	using namespace genes_ui;

	GenesTabState& st = m_genes_;
	if (!st.initialised)
	{
		st.closed_groups = 1u << static_cast<unsigned>(GeneGroup::Colour);   // colours start folded
		st.initialised = true;
	}

	GnCtx g{ st, ctx, protozoa, m_sel_cell_idx_, m_sel_spring_idx_, m_sel_is_spring_,
		m_cell_order_, m_cell_rank_, m_spring_order_, m_spring_rank_, m_sorted_cell_keys_ };

	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	const float main_w = std::max(ImGui::GetContentRegionAvail().x - k_inspector_w - spacing, k_min_main_w);

	// Left column: toolbar, summary chips, then the two matrices side by side
	ImGui::BeginChild("GN_main", ImVec2(main_w, -1.f), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	draw_toolbar(st);
	draw_summary(g);

	const float total_w = ImGui::GetContentRegionAvail().x - spacing;
	const TableWidths widths = split_table_widths(entity_count<CellKind>(g), entity_count<TendonKind>(g), total_w);

	draw_table<CellKind>(g, "GN_cells_child", "GN_cells", "CELLS", widths.cells);
	ImGui::SameLine();
	draw_table<TendonKind>(g, "GN_springs_child", "GN_springs", "SPRINGS", widths.springs);
	ImGui::EndChild();

	ImGui::SameLine();

	// Right column: inspector for whichever kind of gene is selected
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, k_card_rounding);
	ImGui::BeginChild("GN_inspector", ImVec2(k_inspector_w, -1.f), true);
	ImGui::PopStyleVar();
	if (st.sel_is_tendon) draw_inspector_body<TendonKind>(g);
	else                  draw_inspector_body<CellKind>(g);
	ImGui::EndChild();
}