#pragma once
// Shared declarations for the Genes tab. Internal to this folder: nothing outside genes/ should include it.
//
// File map
//   genes_tab.cpp        top-level layout, toolbar, summary chips
//   genes_matrix.cpp     the two gene x entity matrices (CELLS / SPRINGS)
//   genes_inspector.cpp  right-hand inspector: distribution strip, numbers, live edit, wave preview
//   genes_tooltips.cpp   hover tooltips for matrix cells and column headers
//   genes_stats.cpp      per-gene statistics (mean, spread) and limit / spawn flags
//   genes_ui_util.cpp    value formatting and the small drawing helpers shared by the files above
//
// The matrix, inspector, tooltip and stats code is written once as templates over a "kind"
// (CellKind or TendonKind) and explicitly instantiated for both in its own .cpp.

#include "../organism_tab.h"

#include <SFML/Graphics/Color.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <entities/cell/cell.h>
#include <entities/genome/cell_genome.h>
#include <entities/genome/gene_info.h>
#include <entities/genome/tendon_genome.h>
#include <entities/spring/tendon.h>
#include <imgui.h>
#include <simulation/context/sim_command.h>
#include <vector>

namespace genes_ui
{
	// ── Layout (pixels) ──────────────────────────────────────────────────────
	inline constexpr float k_row_h = 18.f;
	inline constexpr float k_cell_h = k_row_h - 2.f;   // CellPadding.y is 1 on each side
	inline constexpr float k_name_w = 134.f;
	inline constexpr float k_col_w = 60.f;
	inline constexpr float k_mean_w = 58.f;
	inline constexpr float k_spread_w = 62.f;
	inline constexpr float k_inspector_w = 340.f;
	inline constexpr float k_min_main_w = 320.f;       // the matrices never get narrower than this
	inline constexpr float k_min_table_w = 300.f;
	inline constexpr float k_table_extra = 18.f;       // child padding + vertical scrollbar
	inline constexpr float k_strip_h = 40.f;
	inline constexpr float k_card_rounding = 4.f;
	inline constexpr float k_prop_label_w = 64.f;      // label column of "Label   value" property rows

	// ── Limits / thresholds ──────────────────────────────────────────────────
	inline constexpr int   k_max_entities = 60;        // columns drawn per matrix, the rest are not shown
	inline constexpr int   k_max_genes = 32;           // >= gene_count of every genome
	inline constexpr int   k_wave_samples = 192;
	inline constexpr float k_limit_eps = 0.012f;       // normalised distance from 0 / 1 that counts as "at a limit"
	inline constexpr float k_spawn_eps = 0.004f;       // slack before a gene counts as "outside its spawn range"
	inline constexpr float k_converged = 0.03f;        // spread below this = converged
	inline constexpr float k_two_pi = 6.28318530718f;
	inline constexpr float k_pi = 3.14159265f;

	// ── Palette ──────────────────────────────────────────────────────────────
	inline constexpr ImVec4 k_accent = { 0.55f, 0.78f, 1.00f, 1.f };
	inline constexpr ImVec4 k_orange = { 1.00f, 0.70f, 0.25f, 1.f };
	inline constexpr ImVec4 k_derived = { 0.65f, 0.90f, 0.70f, 1.f };
	inline constexpr ImVec4 k_good = { 0.30f, 0.85f, 0.35f, 1.f };
	inline constexpr ImVec4 k_value_text = { 0.86f, 0.88f, 0.94f, 1.f };

	static_assert(CellGenome::gene_count <= k_max_genes && TendonGenome::gene_count <= k_max_genes, "raise k_max_genes");
	static_assert(static_cast<int>(GeneGroup::Count) <= 32, "closed_groups is a 32-bit mask");

	// ── Context handed to every helper ───────────────────────────────────────
	struct GnCtx
	{
		GenesTabState& st;
		ImGuiContext& ctx;
		const OrganismTracker& t;
		int& sel_cell;
		int& sel_spring;
		bool& sel_is_spring;
		const std::vector<int>& cell_order;          // cell_order[k]   = tracker index of "Cell k"
		const std::vector<int>& cell_rank;           // cell_rank[i]    = k of the cell at tracker index i
		const std::vector<int>& spring_order;
		const std::vector<int>& spring_rank;
		const std::vector<uint32_t>& cell_keys;      // sorted absolute cell ids
	};

	// Per-entity absolute caps used to turn a 0..1 gene into real units (springs have none)
	struct Maxes { float energy = 0.f, integrity = 0.f, nutrients = 0.f; };

	// ── Kind adaptors: everything that differs between cells and springs ─────
	struct CellKind
	{
		using Genome = CellGenome;
		static constexpr bool        is_tendon = false;
		static constexpr char        prefix = 'C';
		static constexpr const char* noun = "Cell";
		static constexpr const char* plural = "cells";
		static constexpr GeneGroup   wave_group = GeneGroup::FrictionWave;

		static const auto& info() { return k_cell_gene_info; }
		static int count(const OrganismTracker& t) { return std::min<int>(t.cell_count, static_cast<int>(t.cells.size())); }
		static const Genome& genome(const OrganismTracker& t, const int i) { return t.cells[i]; }
		static uint32_t id(const OrganismTracker& t, const int i) { return t.cells[i].id_; }
		static float phase(const OrganismTracker& t, const int i) { return t.cells[i].phase_; }
		static sf::Color outer(const OrganismTracker& t, const int i) { return t.cells[i].outer_color(); }
		static sf::Color inner(const OrganismTracker& t, const int i) { return t.cells[i].inner_color(); }

		// Each cell has its own caps (tied to its mass), so absolute values are read per cell
		static Maxes maxes(const OrganismTracker& t, const int i)
		{
			const Cell& c = t.cells[i];
			return { c.max_energy, c.max_integrity, c.max_nutrients };
		}

		static const std::vector<int>& order(const GnCtx& g) { return g.cell_order; }
		static const std::vector<int>& rank(const GnCtx& g) { return g.cell_rank; }
		static int target(const GnCtx& g) { return g.sel_cell; }
		static bool selected(const GnCtx& g, const int i) { return !g.sel_is_spring && g.sel_cell == i; }
		static void select(GnCtx& g, const int i) { g.sel_cell = i; g.sel_is_spring = false; }
		static CommandType command() { return CommandType::SetCellGene; }

		// Level of the grip wave `k` ticks from now, driven by the live phase (so tendon sync is included)
		static float wave_level(const Genome& gn, const float phase, const float k)
		{
			using G = CellGenome;
			const float s = std::sin(phase + gn.get<G::Offset>() + gn.get<G::Frequency>() * k);
			return std::clamp(gn.get<G::VerticalShift>() + gn.get<G::Amplitude>() * s, 0.f, 1.f);
		}
	};

	struct TendonKind
	{
		using Genome = TendonGenome;
		static constexpr bool        is_tendon = true;
		static constexpr char        prefix = 'S';
		static constexpr const char* noun = "Spring";
		static constexpr const char* plural = "springs";
		static constexpr GeneGroup   wave_group = GeneGroup::ExtensionWave;

		static const auto& info() { return k_tendon_gene_info; }
		static int count(const OrganismTracker& t) { return std::min<int>(t.tendon_count, static_cast<int>(t.springs.size())); }
		static const Genome& genome(const OrganismTracker& t, const int i) { return t.springs[i].genome; }
		static uint32_t id(const OrganismTracker& t, const int i) { return static_cast<uint32_t>(t.springs[i].id_); }
		static float phase(const OrganismTracker& t, const int i) { return t.springs[i].phase_; }
		static sf::Color outer(const OrganismTracker& t, const int i) { return t.springs[i].genome.outer_color(); }
		static sf::Color inner(const OrganismTracker& t, const int i) { return t.springs[i].genome.inner_color(); }

		static Maxes maxes(const OrganismTracker&, const int) { return {}; }

		static const std::vector<int>& order(const GnCtx& g) { return g.spring_order; }
		static const std::vector<int>& rank(const GnCtx& g) { return g.spring_rank; }
		static int target(const GnCtx& g) { return g.sel_spring; }
		static bool selected(const GnCtx& g, const int i) { return g.sel_is_spring && g.sel_spring == i; }
		static void select(GnCtx& g, const int i) { g.sel_spring = i; g.sel_is_spring = true; }
		static CommandType command() { return CommandType::SetTendonGene; }

		static float wave_level(const Genome& gn, const float phase, const float k)
		{
			using G = TendonGenome;
			const float s = std::sin(phase + gn.get<G::Offset>() + gn.get<G::Frequency>() * k);
			return std::clamp(gn.get<G::VerticalShift>() + gn.get<G::Amplitude>() * s, 0.f, 1.f);
		}
	};

	// How many columns a matrix draws for this kind
	template <class K>
	int entity_count(const GnCtx& g)
	{
		return std::min({ K::count(g.t), static_cast<int>(K::order(g).size()), k_max_entities });
	}

	// ── Reading genes ────────────────────────────────────────────────────────
	// One entity's value for one gene: real units, raw 0..1 gene value, and mutation strength.
	struct Sample { float pheno, norm, sigma; };

	template <class K>
	Sample read(const OrganismTracker& t, const int idx, const size_t gene)
	{
		const auto& gn = K::genome(t, idx);
		return { gn.phenotype(gene), gn.gene_value(gene), gn.gene_mutation_strength(gene) };
	}

	// Position of a mutation strength inside its allowed range, on the same log scale Gene::mutate uses
	float sigma_pos(float sigma);

	// Where a sample's bar ends in the current metric (0..1)
	inline float metric_pos(const Sample& s, const GnMetric metric)
	{
		return metric == GnMetric::Sigma ? sigma_pos(s.sigma) : s.norm;
	}

	// Per-gene summary across the whole organism
	struct RowStats
	{
		float mean_norm = 0.f;    // circular mean for wrapping genes
		float mean_pheno = 0.f;
		float mean_sigma = 0.f;
		float mean_pos = 0.f;     // bar position of the mean in the current metric
		float spread = 0.f;       // 0 = identical across the organism, 1 = as different as possible
	};

	// Where one gene sits relative to its limits and to the generation-0 spawn range
	struct GeneFlags { bool at_min = false, at_max = false, above_spawn = false, below_spawn = false; };

	// ── genes_stats.cpp ──────────────────────────────────────────────────────
	template <class K> RowStats compute_stats(const GnCtx& g, size_t gene, int entity_total);
	template <class K> GeneFlags flags_for(const OrganismTracker& t, int idx, size_t gene, float norm);

	// ── genes_ui_util.cpp: formatting ────────────────────────────────────────
	void format_value(char* buf, size_t n, const GeneInfo& info, GnMetric metric, float pheno, float norm, float sigma);
	// Plain-language meaning of a value (period in frames, absolute energy, ...). False when the gene has none.
	bool derived_text(const GeneInfo& info, float value, const Maxes& maxes, char* out, size_t n);
	bool icontains(const char* haystack, const char* needle);

	// ── genes_ui_util.cpp: colours ───────────────────────────────────────────
	ImU32  col32(const sf::Color& c, int alpha);
	ImU32  bright32(const sf::Color& c, int alpha);    // genome colours can be very dark: lift them for the dark UI
	ImVec4 bright4(const sf::Color& c, float alpha);
	ImU32  scale_alpha(ImU32 color, float factor);
	ImU32  group_accent(int group, int alpha);         // a stable hue per gene group
	ImU32  slot_bg(bool row_selected, bool column_selected);
	ImU32  spread_fill(float spread);

	// ── genes_ui_util.cpp: drawing primitives ────────────────────────────────
	void draw_chip(ImDrawList* dl, ImVec2 p, float size, const sf::Color& upper, const sf::Color& lower);
	void draw_inline_chip(const sf::Color& outer, const sf::Color& inner);    // chip sized for a text line; follow with SameLine
	void draw_fold_arrow(ImDrawList* dl, ImVec2 p, bool open);
	void paint_cell(ImDrawList* dl, ImVec2 p0, ImVec2 p1, ImU32 background, float pos, ImU32 fill, const char* text, bool outline);
	void draw_range_bar(ImDrawList* dl, float width, float height, float value_norm, const Range& spawn_norm, ImU32 marker);
	void draw_flag_lines(const GeneFlags& flags);

	// ── genes_ui_util.cpp: layout helpers ────────────────────────────────────
	void draw_section_title(const char* title);
	void draw_property(const char* label, const char* fmt, ...) IM_FMTARGS(2);
	void draw_property_colored(const char* label, const ImVec4& color, const char* fmt, ...) IM_FMTARGS(3);
	// Joined rounded buttons; returns true when the selection changed.
	bool segmented_control(const char* id, const char* const* labels, int count, int& selected);

	// ── genes_tooltips.cpp ───────────────────────────────────────────────────
	template <class K> void gene_tooltip(const GnCtx& g, int idx, int k, size_t gene, const Sample& s);
	template <class K> void entity_tooltip(const GnCtx& g, int idx, int k, int entity_total);

	// ── genes_matrix.cpp ─────────────────────────────────────────────────────
	template <class K> void draw_table(GnCtx& g, const char* child_id, const char* table_id, const char* title, float width);

	// ── genes_inspector.cpp ──────────────────────────────────────────────────
	template <class K> void draw_inspector_body(GnCtx& g);
}