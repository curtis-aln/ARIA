// Genes tab: hover tooltips for matrix cells and for the entity column headers.
#include "genes_ui_common.h"

#include <cstdio>

namespace genes_ui
{
	namespace
	{
		constexpr float k_tooltip_bar_w = 230.f;
		constexpr float k_tooltip_bar_h = 8.f;

		// Position of an absolute cell id among this organism's sorted cells, or -1 if it isn't one of them
		int relative_cell_index(const GnCtx& g, const uint32_t abs_id)
		{
			const auto it = std::lower_bound(g.cell_keys.begin(), g.cell_keys.end(), abs_id);
			return (it != g.cell_keys.end() && *it == abs_id) ? static_cast<int>(it - g.cell_keys.begin()) : -1;
		}

		// Chip + accent-coloured title on one line
		void draw_tooltip_title(const sf::Color& outer, const sf::Color& inner, const char* title)
		{
			draw_inline_chip(outer, inner);
			ImGui::SameLine(0.f, 4.f);
			ImGui::TextColored(k_accent, "%s", title);
		}

		// "3" for a known cell, "?" for one that isn't in this organism
		void format_cell_ref(char* out, const size_t n, const int relative_index)
		{
			if (relative_index >= 0)
				std::snprintf(out, n, "%d", relative_index);
			else
				std::snprintf(out, n, "?");
		}
	}

	// Tooltip for one matrix cell: where the value sits in its limits, plus every number for it
	template <class K>
	void gene_tooltip(const GnCtx& g, const int idx, const int k, const size_t gene, const Sample& s)
	{
		const GeneInfo& info = K::info()[gene];
		const auto& genome = K::genome(g.t, idx);
		const Range limits = genome.limits(gene);
		const Range spawn = genome.gene_initial_range(gene);   // normalised 0..1
		const float span = limits.max - limits.min;

		ImGui::BeginTooltip();

		char title[64];
		std::snprintf(title, sizeof title, "%s  -  %s %d", info.name, K::noun, k);
		draw_tooltip_title(K::outer(g.t, idx), K::inner(g.t, idx), title);

		draw_range_bar(ImGui::GetWindowDrawList(), k_tooltip_bar_w, k_tooltip_bar_h, s.norm, spawn,
			bright32(K::outer(g.t, idx), 255));

		draw_property("Value", "%.5f", s.pheno);
		char derived[64];
		if (derived_text(info, s.pheno, K::maxes(g.t, idx), derived, sizeof derived))
			draw_property_colored("", k_derived, "%s", derived);
		draw_property("Gene", "%.4f", s.norm);
		draw_property("Sigma", "%.4f", s.sigma);
		draw_property("Limits", "%.4g .. %.4g", limits.min, limits.max);
		draw_property("Spawn", "%.4g .. %.4g", limits.min + spawn.min * span, limits.min + spawn.max * span);
		draw_flag_lines(flags_for<K>(g.t, idx, gene, s.norm));

		ImGui::EndTooltip();
	}

	// Tooltip for a column header: which entity it is and how far its genome is from the others
	template <class K>
	void entity_tooltip(const GnCtx& g, const int idx, const int k, const int entity_total)
	{
		const auto& genome = K::genome(g.t, idx);
		const auto& order = K::order(g);

		ImGui::BeginTooltip();

		char title[64];
		std::snprintf(title, sizeof title, "%s %d  (id %u)", K::noun, k, K::id(g.t, idx));
		draw_tooltip_title(K::outer(g.t, idx), K::inner(g.t, idx), title);

		if constexpr (K::is_tendon)
		{
			// Which two cells this spring connects
			const Tendon& spring = g.t.springs[idx];
			char a[8], b[8];
			format_cell_ref(a, sizeof a, relative_cell_index(g, spring.cell_A_id));
			format_cell_ref(b, sizeof b, relative_cell_index(g, spring.cell_B_id));
			draw_property("Joins", "Cell %s  ->  Cell %s", a, b);
		}
		else
		{
			draw_property("Generation", "%u", genome.generation());
		}

		draw_property("Mean sigma", "%.4f", genome.mean_mutation_strength(K::Genome::SharedCount, K::Genome::gene_count));

		if (entity_total >= 2)
		{
			float distance_sum = 0.f;
			for (int j = 0; j < entity_total; ++j)
				if (j != k)
					distance_sum += genome.distance(K::genome(g.t, order[j]));
			draw_property("Distance", "%.3f  (mean to others)", distance_sum / static_cast<float>(entity_total - 1));
		}

		ImGui::EndTooltip();
	}

	template void gene_tooltip<CellKind>(const GnCtx&, int, int, size_t, const Sample&);
	template void gene_tooltip<TendonKind>(const GnCtx&, int, int, size_t, const Sample&);
	template void entity_tooltip<CellKind>(const GnCtx&, int, int, int);
	template void entity_tooltip<TendonKind>(const GnCtx&, int, int, int);
}