// Genes tab: the gene x entity matrices. Genes run down the rows, entities (cells or springs) across the columns,
// followed by a Mean and a Spread column.
#include "genes_ui_common.h"

#include <array>
#include <cstdio>

namespace genes_ui
{
	namespace
	{
		// ── Colours ──────────────────────────────────────────────────────────
		constexpr ImU32 k_col_title = IM_COL32(150, 185, 255, 255);
		constexpr ImU32 k_col_header_dim = IM_COL32(150, 156, 175, 255);
		constexpr ImU32 k_col_header_label = IM_COL32(200, 205, 220, 255);
		constexpr ImU32 k_col_header_selected = IM_COL32(70, 90, 150, 110);
		constexpr ImU32 k_col_header_hover = IM_COL32(255, 255, 255, 18);
		constexpr ImU32 k_col_group_row = IM_COL32(34, 37, 46, 255);
		constexpr ImU32 k_col_group_count = IM_COL32(120, 126, 145, 255);
		constexpr ImU32 k_col_limit_edge = IM_COL32(255, 175, 60, 255);
		constexpr ImU32 k_col_spawn_mark = IM_COL32(90, 200, 235, 255);
		constexpr ImU32 k_col_mean_fill = IM_COL32(150, 160, 190, 55);

		constexpr float k_name_indent = 8.f;
		constexpr float k_chip_size = 11.f;

		constexpr ImGuiTableFlags k_table_flags = ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY
			| ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_SizingFixedFit;

		using StatsTable = std::array<RowStats, k_max_genes>;

		// A gene that survived the filters, with the spread used for sorting
		struct RowRef { int gene; float spread; };
		using RowList = std::array<RowRef, k_max_genes>;

		// ── Selection ────────────────────────────────────────────────────────
		// Selecting a row, cell or stat cell all point the inspector at the same gene
		void select_gene(GenesTabState& st, const bool is_tendon, const size_t gene)
		{
			st.sel_is_tendon = is_tendon;
			st.sel_gene = static_cast<int>(gene);
		}

		// ── Header row ───────────────────────────────────────────────────────
		// Left-aligned label inside the current header cell
		void draw_header_label(ImDrawList* dl, const char* text, const ImU32 color)
		{
			const ImVec2 p0 = ImGui::GetCursorScreenPos();
			const float width = ImGui::GetContentRegionAvail().x;
			const ImVec2 text_size = ImGui::CalcTextSize(text);
			dl->AddText({ p0.x + 2.f, p0.y + (k_row_h - text_size.y) * 0.5f + 1.f }, color, text);
			ImGui::Dummy({ width, k_row_h });
		}

		template <class K>
		void setup_columns(const int entity_total)
		{
			ImGui::TableSetupColumn("Gene", ImGuiTableColumnFlags_WidthFixed, k_name_w);
			for (int k = 0; k < entity_total; ++k)
			{
				char name[8];
				std::snprintf(name, sizeof name, "%c%d", K::prefix, k);
				ImGui::TableSetupColumn(name, ImGuiTableColumnFlags_WidthFixed, k_col_w);
			}
			ImGui::TableSetupColumn("Mean", ImGuiTableColumnFlags_WidthFixed, k_mean_w);
			ImGui::TableSetupColumn("Spread", ImGuiTableColumnFlags_WidthFixed, k_spread_w);
			ImGui::TableSetupScrollFreeze(1, 1);
		}

		// One clickable header cell per entity: colour chip + "C3" label. The selected one gets an underline in its own colour.
		template <class K>
		void draw_entity_header(GnCtx& g, ImDrawList* dl, const int k, const int idx, const int entity_total)
		{
			const OrganismTracker& t = g.t;
			ImGui::TableSetColumnIndex(1 + k);
			ImGui::PushID(k);

			const ImVec2 p0 = ImGui::GetCursorScreenPos();
			const float width = ImGui::GetContentRegionAvail().x;
			const ImVec2 p1{ p0.x + width, p0.y + k_row_h };
			const bool clicked = ImGui::InvisibleButton("##header", { width, k_row_h });
			const bool hovered = ImGui::IsItemHovered();

			if (K::selected(g, idx))
			{
				dl->AddRectFilled(p0, p1, k_col_header_selected);
				dl->AddRectFilled({ p0.x, p1.y - 2.f }, p1, bright32(K::outer(t, idx), 230));
			}
			else if (hovered)
				dl->AddRectFilled(p0, p1, k_col_header_hover);

			draw_chip(dl, { p0.x + 4.f, p0.y + (k_row_h - k_chip_size) * 0.5f }, k_chip_size, K::outer(t, idx), K::inner(t, idx));

			char label[8];
			std::snprintf(label, sizeof label, "%c%d", K::prefix, k);
			const ImVec2 label_size = ImGui::CalcTextSize(label);
			dl->AddText({ p0.x + 19.f, p0.y + (k_row_h - label_size.y) * 0.5f }, k_col_header_label, label);

			if (clicked)
				K::select(g, idx);
			if (hovered)
				entity_tooltip<K>(g, idx, k, entity_total);
			ImGui::PopID();
		}

		// Title cell, one header per entity, then the Mean and Spread headers
		template <class K>
		void draw_header_row(GnCtx& g, ImDrawList* dl, const char* title, const int entity_total)
		{
			const auto& order = K::order(g);
			ImGui::TableNextRow(ImGuiTableRowFlags_Headers, k_row_h + 2.f);

			ImGui::TableSetColumnIndex(0);
			char heading[40];
			std::snprintf(heading, sizeof heading, "%s  (%d)", title, entity_total);
			draw_header_label(dl, heading, k_col_title);

			for (int k = 0; k < entity_total; ++k)
				draw_entity_header<K>(g, dl, k, order[k], entity_total);

			ImGui::TableSetColumnIndex(1 + entity_total);
			draw_header_label(dl, "Mean", k_col_header_dim);
			ImGui::TableSetColumnIndex(2 + entity_total);
			draw_header_label(dl, "Spread", k_col_header_dim);
		}

		// ── Row selection ────────────────────────────────────────────────────
		// Genes of one group that pass the filter / hide-converged settings, sorted by spread if asked. Returns the count.
		template <class K>
		int collect_group_rows(const GnCtx& g, const int group, const int entity_total, const StatsTable& stats,
			const bool filter_active, RowList& rows)
		{
			const GenesTabState& st = g.st;
			const auto& infos = K::info();

			int count = 0;
			for (size_t i = 0; i < infos.size(); ++i)
			{
				if (static_cast<int>(infos[i].group) != group)
					continue;
				if (filter_active && !icontains(infos[i].name, st.filter))
					continue;
				if (st.hide_converged&& entity_total >= 2 && stats[i].spread < k_converged)
					continue;
				rows[count++] = { static_cast<int>(i), stats[i].spread };
			}

			if (st.sort_by_spread)
				std::stable_sort(rows.begin(), rows.begin() + count,
					[](const RowRef& a, const RowRef& b) { return a.spread > b.spread; });
			return count;
		}

		// ── Group header ─────────────────────────────────────────────────────
		// Fold / unfold row with a coloured accent stripe and the row count. Returns true when the group is folded.
		// A filter forces every group open so matches are never hidden.
		bool draw_group_header(ImDrawList* dl, GenesTabState& st, const int group, const int row_count, const bool filter_active)
		{
			const bool closed = !filter_active && ((st.closed_groups >> group) & 1u);

			ImGui::TableNextRow(ImGuiTableRowFlags_None, k_row_h);
			ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, k_col_group_row);
			ImGui::TableSetColumnIndex(0);
			ImGui::PushID(1000 + group);

			const ImVec2 p0 = ImGui::GetCursorScreenPos();
			const float column_w = ImGui::GetContentRegionAvail().x;

			char label[48];
			std::snprintf(label, sizeof label, "     %s", gene_group_name(static_cast<GeneGroup>(group)));
			if (ImGui::Selectable(label, false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, k_cell_h)))
				st.closed_groups ^= (1u << group);

			dl->AddRectFilled({ p0.x - 3.f, p0.y }, { p0.x, p0.y + k_cell_h }, group_accent(group, 255));
			draw_fold_arrow(dl, { p0.x + 4.f, p0.y + 4.f }, !closed);

			char count_text[8];
			std::snprintf(count_text, sizeof count_text, "%d", row_count);
			const ImVec2 count_size = ImGui::CalcTextSize(count_text);
			dl->AddText({ p0.x + column_w - count_size.x - 4.f, p0.y + (k_cell_h - count_size.y) * 0.5f },
				k_col_group_count, count_text);

			ImGui::PopID();
			return closed;
		}

		// ── Gene row ─────────────────────────────────────────────────────────
		// Gene name cell: click selects the row, hover shows the gene's description
		void draw_name_cell(GenesTabState& st, const GeneInfo& info, const bool is_tendon, const size_t gene, const bool row_selected)
		{
			ImGui::TableSetColumnIndex(0);
			ImGui::Indent(k_name_indent);
			if (ImGui::Selectable(info.name, row_selected, ImGuiSelectableFlags_None, ImVec2(0.f, k_cell_h)))
				select_gene(st, is_tendon, gene);
			ImGui::Unindent(k_name_indent);

			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::PushTextWrapPos(320.f);
				ImGui::TextColored(k_accent, "%s", info.name);
				ImGui::TextWrapped("%s", info.tip);
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}
		}

		// Orange edge when a gene sits at a limit, small triangle when it is outside its spawn range
		void draw_cell_flags(ImDrawList* dl, const ImVec2 p0, const ImVec2 p1, const GeneFlags& flags)
		{
			if (flags.at_min) dl->AddRectFilled(p0, { p0.x + 2.f, p1.y }, k_col_limit_edge);
			if (flags.at_max) dl->AddRectFilled({ p1.x - 2.f, p0.y }, p1, k_col_limit_edge);

			if (flags.above_spawn)
				dl->AddTriangleFilled({ p0.x + 4.f, p0.y + 7.f }, { p0.x + 10.f, p0.y + 7.f }, { p0.x + 7.f, p0.y + 3.f }, k_col_spawn_mark);
			else if (flags.below_spawn)
				dl->AddTriangleFilled({ p0.x + 4.f, p0.y + 3.f }, { p0.x + 10.f, p0.y + 3.f }, { p0.x + 7.f, p0.y + 7.f }, k_col_spawn_mark);
		}

		// One value cell: bar + number, optional flags, click to select, hover for the tooltip
		template <class K>
		void draw_value_cell(GnCtx& g, ImDrawList* dl, const int k, const int idx, const size_t gene,
			const GeneInfo& info, const bool row_selected)
		{
			GenesTabState& st = g.st;
			ImGui::TableSetColumnIndex(1 + k);

			const Sample sample = read<K>(g.t, idx, gene);
			char text[24];
			format_value(text, sizeof text, info, st.metric, sample.pheno, sample.norm, sample.sigma);

			const bool column_selected = K::selected(g, idx);
			const ImVec2 p0 = ImGui::GetCursorScreenPos();
			const float width = ImGui::GetContentRegionAvail().x;
			const ImVec2 p1{ p0.x + width, p0.y + k_cell_h };

			ImGui::PushID(k);
			const bool clicked = ImGui::InvisibleButton("##value", { width, k_cell_h });
			const bool hovered = ImGui::IsItemHovered();
			ImGui::PopID();

			paint_cell(dl, p0, p1, slot_bg(row_selected, column_selected), metric_pos(sample, st.metric),
				bright32(K::outer(g.t, idx), 85), text, row_selected && column_selected);

			if (st.show_flags)
				draw_cell_flags(dl, p0, p1, flags_for<K>(g.t, idx, gene, sample.norm));

			if (clicked)
			{
				K::select(g, idx);
				select_gene(st, K::is_tendon, gene);
			}
			if (hovered)
				gene_tooltip<K>(g, idx, k, gene, sample);
		}

		// Mean / Spread cell: a bar plus a number; clicking selects the row
		void draw_stat_cell(GenesTabState& st, ImDrawList* dl, const int column, const bool is_tendon, const size_t gene,
			const bool row_selected, const float pos, const ImU32 fill, const char* text)
		{
			ImGui::TableSetColumnIndex(column);
			const ImVec2 p0 = ImGui::GetCursorScreenPos();
			const float width = ImGui::GetContentRegionAvail().x;

			ImGui::PushID(column);
			const bool clicked = ImGui::InvisibleButton("##stat", { width, k_cell_h });
			ImGui::PopID();

			paint_cell(dl, p0, { p0.x + width, p0.y + k_cell_h }, slot_bg(row_selected, false), pos, fill, text, false);
			if (clicked)
				select_gene(st, is_tendon, gene);
		}

		// A full gene row: name, one value per entity, Mean, Spread
		template <class K>
		void draw_gene_row(GnCtx& g, ImDrawList* dl, const int entity_total, const size_t gene, const RowStats& stats)
		{
			GenesTabState& st = g.st;
			const GeneInfo& info = K::info()[gene];
			const auto& order = K::order(g);
			const bool row_selected = st.sel_is_tendon == K::is_tendon && st.sel_gene == static_cast<int>(gene);

			ImGui::TableNextRow(ImGuiTableRowFlags_None, k_row_h);
			ImGui::PushID(static_cast<int>(gene));

			draw_name_cell(st, info, K::is_tendon, gene, row_selected);
			for (int k = 0; k < entity_total; ++k)
				draw_value_cell<K>(g, dl, k, order[k], gene, info, row_selected);

			char mean_text[24];
			format_value(mean_text, sizeof mean_text, info, st.metric, stats.mean_pheno, stats.mean_norm, stats.mean_sigma);
			draw_stat_cell(st, dl, 1 + entity_total, K::is_tendon, gene, row_selected, stats.mean_pos, k_col_mean_fill, mean_text);

			char spread_text[16];
			if (entity_total >= 2) std::snprintf(spread_text, sizeof spread_text, "%.2f", stats.spread);
			else                   std::snprintf(spread_text, sizeof spread_text, "-");
			draw_stat_cell(st, dl, 2 + entity_total, K::is_tendon, gene, row_selected,
				entity_total >= 2 ? stats.spread : 0.f, spread_fill(stats.spread), spread_text);

			ImGui::PopID();
		}

		// Every gene group: its header, then its rows unless folded
		template <class K>
		void draw_gene_groups(GnCtx& g, ImDrawList* dl, const int entity_total, const StatsTable& stats, const bool filter_active)
		{
			bool drawn_any = false;

			for (int group = 0; group < static_cast<int>(GeneGroup::Count); ++group)
			{
				RowList rows{};
				const int row_count = collect_group_rows<K>(g, group, entity_total, stats, filter_active, rows);
				if (row_count == 0)
					continue;
				drawn_any = true;

				if (draw_group_header(dl, g.st, group, row_count, filter_active))
					continue;

				for (int r = 0; r < row_count; ++r)
				{
					const size_t gene = static_cast<size_t>(rows[r].gene);
					draw_gene_row<K>(g, dl, entity_total, gene, stats[gene]);
				}
			}

			if (!drawn_any)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextDisabled("no genes match");
			}
		}
	}

	// ─────────────────────────────────────────────────────────────────────────
	//  The matrix card
	// ─────────────────────────────────────────────────────────────────────────
	template <class K>
	void draw_table(GnCtx& g, const char* child_id, const char* table_id, const char* title, const float width)
	{
		const auto& infos = K::info();
		const int entity_total = entity_count<K>(g);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.f, 2.f));
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, k_card_rounding);
		ImGui::BeginChild(child_id, ImVec2(width, -1.f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		ImGui::PopStyleVar(2);

		if (entity_total == 0)
		{
			ImGui::TextDisabled("No %s", K::plural);
			ImGui::EndChild();
			return;
		}

		// Statistics for every gene up front: rows need them for sorting and the Mean / Spread columns
		StatsTable stats{};
		for (size_t i = 0; i < infos.size(); ++i)
			stats[i] = compute_stats<K>(g, i, entity_total);

		const bool filter_active = g.st.filter[0] != '\0';

		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(3.f, 1.f));
		if (ImGui::BeginTable(table_id, entity_total + 3, k_table_flags))
		{
			setup_columns<K>(entity_total);
			ImDrawList* dl = ImGui::GetWindowDrawList();
			draw_header_row<K>(g, dl, title, entity_total);
			draw_gene_groups<K>(g, dl, entity_total, stats, filter_active);
			ImGui::EndTable();
		}
		ImGui::PopStyleVar();
		ImGui::EndChild();
	}

	template void draw_table<CellKind>(GnCtx&, const char*, const char*, const char*, float);
	template void draw_table<TendonKind>(GnCtx&, const char*, const char*, const char*, float);
}