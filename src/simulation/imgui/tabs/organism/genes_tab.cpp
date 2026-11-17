#include "organism_tab.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <entities/cell/cell.h>
#include <entities/cell/cell_settings.h>
#include <entities/genome/cell_genome.h>
#include <entities/genome/gene_info.h>
#include <entities/genome/tendon_genome.h>
#include <entities/spring/tendon.h>
#include <imgui.h>
#include <implot.h>
#include <simulation/context/sim_command.h>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  Genes tab
//
//  Left:  two matrices side by side. Genes run down the rows, entities across the columns.
//         CELLS shows every cell's CellGenome, SPRINGS shows every tendon's own TendonGenome.
//  Right: inspector for the selected gene (distribution strip, numbers, live edit, wave overlay).
//
//  Everything is read straight from the snapshot each frame, nothing is cached between frames
//  except the view settings in GenesTabState (organism_tab.h).
// ─────────────────────────────────────────────────────────────────────────────
namespace
{
	// ── Layout ───────────────────────────────────────────────────────────────
	constexpr float k_row_h = 18.f;
	constexpr float k_cell_h = k_row_h - 2.f;   // CellPadding.y is 1 on each side
	constexpr float k_name_w = 134.f;
	constexpr float k_col_w = 60.f;
	constexpr float k_mean_w = 58.f;
	constexpr float k_spread_w = 62.f;
	constexpr float k_inspector_w = 340.f;
	constexpr float k_min_table_w = 300.f;
	constexpr float k_table_extra = 18.f;            // child padding + vertical scrollbar
	constexpr float k_strip_h = 40.f;

	// ── Limits / thresholds ──────────────────────────────────────────────────
	constexpr int   k_max_entities = 60;              // columns drawn per matrix, the rest are not shown
	constexpr int   k_max_genes = 32;              // >= gene_count of every genome
	constexpr int   k_wave_samples = 192;
	constexpr float k_limit_eps = 0.012f;          // normalised distance from 0 / 1 that counts as "at a limit"
	constexpr float k_spawn_eps = 0.004f;          // slack before a gene counts as "outside its spawn range"
	constexpr float k_converged = 0.03f;           // spread below this = converged
	constexpr float k_two_pi = 6.28318530718f;

	constexpr ImVec4 k_accent = { 0.55f, 0.78f, 1.00f, 1.f };
	constexpr ImVec4 k_orange = { 1.00f, 0.70f, 0.25f, 1.f };
	constexpr ImVec4 k_derived = { 0.65f, 0.90f, 0.70f, 1.f };

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
		const std::vector<int>& cell_order;     // cell_order[k]   = tracker index of "Cell k"
		const std::vector<int>& cell_rank;      // cell_rank[i]    = k of the cell at tracker index i
		const std::vector<int>& spring_order;
		const std::vector<int>& spring_rank;
		const std::vector<uint32_t>& cell_keys;      // sorted absolute cell ids
	};

	int rel_cell(const GnCtx& g, const uint32_t abs_id)
	{
		const auto it = std::lower_bound(g.cell_keys.begin(), g.cell_keys.end(), abs_id);
		return (it != g.cell_keys.end() && *it == abs_id) ? static_cast<int>(it - g.cell_keys.begin()) : -1;
	}

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

	template <class K>
	int entity_count(const GnCtx& g)
	{
		return std::min({ K::count(g.t), static_cast<int>(K::order(g).size()), k_max_entities });
	}

	// ── Reading genes ────────────────────────────────────────────────────────
	struct Sample { float pheno, norm, sigma; };

	template <class K>
	Sample read(const OrganismTracker& t, const int idx, const size_t gene)
	{
		const auto& gn = K::genome(t, idx);
		return { gn.phenotype(gene), gn.gene_value(gene), gn.gene_mutation_strength(gene) };
	}

	// Position of a mutation strength inside its allowed range, on the same log scale Gene::mutate uses
	float sigma_pos(const float sigma)
	{
		const float lo = std::log(GeneSettings::mutation_strength_range.min);
		const float hi = std::log(GeneSettings::mutation_strength_range.max);
		return std::clamp((std::log(std::max(sigma, 1e-6f)) - lo) / (hi - lo), 0.f, 1.f);
	}

	float metric_pos(const Sample& s, const GnMetric m)
	{
		return m == GnMetric::Sigma ? sigma_pos(s.sigma) : s.norm;
	}

	struct RowStats
	{
		float mean_norm = 0.f;   // circular mean for wrapping genes
		float mean_pheno = 0.f;
		float mean_sigma = 0.f;
		float mean_pos = 0.f;   // bar position of the mean in the current metric
		float spread = 0.f;   // 0 = identical across the organism, 1 = as different as possible
	};

	template <class K>
	RowStats compute_stats(const GnCtx& g, const size_t gene, const int n)
	{
		RowStats r;
		if (n <= 0)
			return r;

		const auto& order = K::order(g);
		const auto& g0 = K::genome(g.t, order[0]);
		const bool  wrap = g0.gene_wraps(gene);
		const Range lim = g0.limits(gene);
		const GnMetric m = g.st.metric;
		const bool  circular = wrap && m != GnMetric::Sigma;

		float sum_pos = 0.f, sum_pos2 = 0.f, sum_pheno = 0.f, sum_sigma = 0.f, sum_norm = 0.f, sx = 0.f, sy = 0.f;
		for (int k = 0; k < n; ++k)
		{
			const Sample s = read<K>(g.t, order[k], gene);
			const float pos = metric_pos(s, m);
			sum_pos += pos;
			sum_pos2 += pos * pos;
			sum_pheno += s.pheno;
			sum_sigma += s.sigma;
			sum_norm += s.norm;
			sx += std::cos(k_two_pi * s.norm);
			sy += std::sin(k_two_pi * s.norm);
		}

		const float inv = 1.f / static_cast<float>(n);
		r.mean_sigma = sum_sigma * inv;
		if (wrap)
		{
			const float turns = std::atan2(sy, sx) / k_two_pi;
			r.mean_norm = turns - std::floor(turns);
			r.mean_pheno = lim.min + r.mean_norm * (lim.max - lim.min);
		}
		else
		{
			r.mean_norm = sum_norm * inv;
			r.mean_pheno = sum_pheno * inv;
		}
		r.mean_pos = (m == GnMetric::Sigma) ? sigma_pos(r.mean_sigma) : r.mean_norm;

		if (n >= 2)
		{
			if (circular)
			{
				// circular standard deviation, in turns, doubled so it lines up with the linear case
				const float R = std::clamp(std::sqrt(sx * sx + sy * sy) * inv, 1e-6f, 1.f);
				r.spread = std::min(1.f, std::sqrt(-2.f * std::log(R)) / k_two_pi * 2.f);
			}
			else
			{
				const float mean = sum_pos * inv;
				const float var = std::max(0.f, sum_pos2 * inv - mean * mean);
				r.spread = std::min(1.f, std::sqrt(var) * 2.f);
			}
		}
		return r;
	}

	// ── Formatting ───────────────────────────────────────────────────────────
	void format_value(char* buf, const size_t n, const GeneInfo& info, const GnMetric m,
		const float pheno, const float norm, const float sigma)
	{
		switch (m)
		{
		case GnMetric::Gene:  std::snprintf(buf, n, "%.3f", norm);  return;
		case GnMetric::Sigma: std::snprintf(buf, n, "%.4f", sigma); return;
		default: break;
		}
		switch (info.fmt)
		{
		case GeneFmt::Whole: std::snprintf(buf, n, "%.0f", pheno); break;
		case GeneFmt::Tenth: std::snprintf(buf, n, "%.1f", pheno); break;
		case GeneFmt::Fine:  std::snprintf(buf, n, "%.4f", pheno); break;
		default:             std::snprintf(buf, n, "%.3f", pheno); break;
		}
	}

	// Plain-language meaning of a value, e.g. period in frames or an absolute energy threshold
	bool derived_text(const GeneInfo& info, const float v, char* out, const size_t n)
	{
		switch (info.derive)
		{
		case GeneDerive::Period:
			if (std::abs(v) < 1e-6f) std::snprintf(out, n, "static, no oscillation");
			else                     std::snprintf(out, n, "period %.1f frames%s", k_two_pi / std::abs(v), v < 0.f ? " (reversed)" : "");
			return true;
		case GeneDerive::EnergyAbs:
			std::snprintf(out, n, "= %.1f energy (max %.0f)", v * CellSettings::max_energy, CellSettings::max_energy);
			return true;
		case GeneDerive::IntegrityAbs:
			std::snprintf(out, n, "= %.1f integrity (max %.0f)", v * CellSettings::max_integrity, CellSettings::max_integrity);
			return true;
		case GeneDerive::NutrientsAbs:
			std::snprintf(out, n, "= %.1f nutrients (max %.0f)", v * CellSettings::max_nutrients, CellSettings::max_nutrients);
			return true;
		case GeneDerive::Percent:
			std::snprintf(out, n, "= %.1f%%", v * 100.f);
			return true;
		case GeneDerive::Degrees:
			std::snprintf(out, n, "= %.0f deg", v * 180.f / 3.14159265f);
			return true;
		case GeneDerive::EffectiveGive:
			std::snprintf(out, n, "effective %.0f%% (sim clamps to 10-90%%)", std::clamp(v, 0.1f, 0.9f) * 100.f);
			return true;
		default:
			return false;
		}
	}

	bool icontains(const char* hay, const char* needle)
	{
		if (!*needle)
			return true;
		const size_t nl = std::strlen(needle);
		for (const char* h = hay; *h; ++h)
		{
			size_t i = 0;
			while (i < nl && h[i] && std::tolower(static_cast<unsigned char>(h[i])) == std::tolower(static_cast<unsigned char>(needle[i])))
				++i;
			if (i == nl)
				return true;
		}
		return false;
	}

	// ── Gene state flags ─────────────────────────────────────────────────────
	struct GeneFlags { bool at_min = false, at_max = false, above_spawn = false, below_spawn = false; };

	template <class K>
	GeneFlags flags_for(const OrganismTracker& t, const int idx, const size_t gene, const float norm)
	{
		const auto& gn = K::genome(t, idx);
		GeneFlags f;
		if (!gn.gene_wraps(gene))
		{
			f.at_min = norm <= k_limit_eps;
			f.at_max = norm >= 1.f - k_limit_eps;
		}
		const Range init = gn.gene_initial_range(gene);
		f.above_spawn = norm > init.max + k_spawn_eps;
		f.below_spawn = norm < init.min - k_spawn_eps;
		return f;
	}

	// ── Drawing helpers ──────────────────────────────────────────────────────
	ImU32 col32(const sf::Color& c, const int a) { return IM_COL32(c.r, c.g, c.b, a); }

	// Genome colours are random and can be very dark, so lift them toward white for use on the dark UI
	ImU32 bright32(const sf::Color& c, const int a)
	{
		const auto lift = [](const uint8_t v) { return static_cast<int>(v * 0.75f + 64.f); };
		return IM_COL32(lift(c.r), lift(c.g), lift(c.b), a);
	}

	ImVec4 bright4(const sf::Color& c, const float a)
	{
		const auto lift = [](const uint8_t v) { return (static_cast<float>(v) / 255.f) * 0.75f + 0.25f; };
		return { lift(c.r), lift(c.g), lift(c.b), a };
	}

	void draw_chip(ImDrawList* dl, const ImVec2 p, const float s, const sf::Color& upper, const sf::Color& lower)
	{
		dl->AddTriangleFilled({ p.x, p.y }, { p.x + s, p.y }, { p.x, p.y + s }, col32(upper, 255));
		dl->AddTriangleFilled({ p.x + s, p.y }, { p.x + s, p.y + s }, { p.x, p.y + s }, col32(lower, 255));
		dl->AddRect(p, { p.x + s, p.y + s }, IM_COL32(0, 0, 0, 140));
	}

	void draw_fold_arrow(ImDrawList* dl, const ImVec2 p, const bool open)
	{
		const ImU32 c = IM_COL32(170, 176, 195, 255);
		if (open) dl->AddTriangleFilled({ p.x, p.y + 1.f }, { p.x + 8.f, p.y + 1.f }, { p.x + 4.f, p.y + 6.f }, c);
		else      dl->AddTriangleFilled({ p.x + 1.f, p.y - 1.f }, { p.x + 1.f, p.y + 9.f }, { p.x + 6.f, p.y + 4.f }, c);
	}

	ImU32 slot_bg(const bool row_sel, const bool col_sel)
	{
		if (row_sel && col_sel) return IM_COL32(40, 46, 66, 255);
		if (row_sel)            return IM_COL32(32, 35, 46, 255);
		if (col_sel)            return IM_COL32(26, 30, 44, 255);
		return IM_COL32(21, 22, 27, 255);
	}

	ImU32 spread_fill(const float spread)
	{
		const float t = std::clamp(spread * 3.f, 0.f, 1.f);
		const auto mix = [t](const float a, const float b) { return static_cast<int>(a + (b - a) * t); };
		return IM_COL32(mix(90.f, 255.f), mix(100.f, 160.f), mix(125.f, 60.f), mix(70.f, 150.f));
	}

	// One matrix cell: dark slot, bar fill from the left, right-aligned number
	void paint_cell(ImDrawList* dl, const ImVec2 p0, const ImVec2 p1, const ImU32 bg,
		const float pos, const ImU32 fill, const char* text, const bool outline)
	{
		dl->AddRectFilled(p0, p1, bg);
		const float w = p1.x - p0.x;
		if (pos > 0.f)
			dl->AddRectFilled(p0, { p0.x + w * std::clamp(pos, 0.f, 1.f), p1.y }, fill);
		const ImVec2 ts = ImGui::CalcTextSize(text);
		dl->AddText({ p1.x - ts.x - 3.f, p0.y + (p1.y - p0.y - ts.y) * 0.5f }, IM_COL32(205, 210, 225, 255), text);
		if (outline)
			dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 200), 0.f, 0, 1.f);
	}

	void tooltip_flags(const GeneFlags& f)
	{
		if (f.at_min)      ImGui::TextColored(k_orange, "At its lower limit");
		if (f.at_max)      ImGui::TextColored(k_orange, "At its upper limit");
		if (f.above_spawn) ImGui::TextColored(k_accent, "Above its generation-0 spawn range");
		if (f.below_spawn) ImGui::TextColored(k_accent, "Below its generation-0 spawn range");
	}

	// ── Tooltips ─────────────────────────────────────────────────────────────
	template <class K>
	void gene_tooltip(const GnCtx& g, const int idx, const int k, const size_t gene, const Sample& s)
	{
		const GeneInfo& info = K::info()[gene];
		const auto& gn = K::genome(g.t, idx);
		const Range lim = gn.limits(gene);
		const Range init = gn.gene_initial_range(gene);
		const float span = lim.max - lim.min;

		ImGui::BeginTooltip();
		ImGui::TextColored(k_accent, "%s  -  %s %d", info.name, K::noun, k);
		ImGui::Text("Value  %.5f", s.pheno);
		char d[64];
		if (derived_text(info, s.pheno, d, sizeof d))
			ImGui::TextColored(k_derived, "       %s", d);
		ImGui::Text("Gene   %.4f    Sigma %.4f", s.norm, s.sigma);
		ImGui::TextDisabled("Limits %.4g .. %.4g", lim.min, lim.max);
		ImGui::TextDisabled("Spawn  %.4g .. %.4g", lim.min + init.min * span, lim.min + init.max * span);
		tooltip_flags(flags_for<K>(g.t, idx, gene, s.norm));
		ImGui::EndTooltip();
	}

	template <class K>
	void entity_tooltip(const GnCtx& g, const int idx, const int k, const int n)
	{
		const auto& gn = K::genome(g.t, idx);
		const auto& order = K::order(g);

		ImGui::BeginTooltip();
		if constexpr (K::is_tendon)
		{
			const Tendon& s = g.t.springs[idx];
			ImGui::TextColored(k_accent, "Spring %d  (id %u)", k, K::id(g.t, idx));
			const int ra = rel_cell(g, s.cell_A_id), rb = rel_cell(g, s.cell_B_id);
			char a[8], b[8];
			if (ra >= 0) std::snprintf(a, sizeof a, "%d", ra); else std::snprintf(a, sizeof a, "?");
			if (rb >= 0) std::snprintf(b, sizeof b, "%d", rb); else std::snprintf(b, sizeof b, "?");
			ImGui::Text("Cell %s  ->  Cell %s", a, b);
		}
		else
		{
			ImGui::TextColored(k_accent, "Cell %d  (id %u)", k, K::id(g.t, idx));
			ImGui::Text("Generation %u", gn.generation());
		}

		ImGui::Text("Mean sigma  %.4f", gn.mean_mutation_strength(K::Genome::SharedCount, K::Genome::gene_count));
		if (n >= 2)
		{
			float sum = 0.f;
			for (int j = 0; j < n; ++j)
				if (j != k)
					sum += gn.distance(K::genome(g.t, order[j]));
			ImGui::Text("Mean genome distance  %.3f", sum / static_cast<float>(n - 1));
		}
		ImGui::EndTooltip();
	}

	// ── Live editing ─────────────────────────────────────────────────────────
	// int_val = gene index, cell_tendon_idx = absolute cell id / tendon id, float_val = phenotype value
	template <class K>
	void push_gene_cmd(GnCtx& g, const uint32_t id, const size_t gene, const float value)
	{
		SimCommand cmd{ .section = CommandSection::CellManagerEvent, .type = K::command() };
		cmd.cell_tendon_idx = static_cast<int>(id);
		cmd.int_val = static_cast<int>(gene);
		cmd.float_val = value;
		g.ctx.push(cmd);
	}

	// ── Toolbar + summary ────────────────────────────────────────────────────
	void draw_toolbar(GenesTabState& st)
	{
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("Show");
		ImGui::SameLine();

		const auto seg = [&](const char* label, const GnMetric m)
			{
				const bool on = st.metric == m;
				if (on)
				{
					const ImVec4 c{ 0.24f, 0.36f, 0.62f, 1.f };
					ImGui::PushStyleColor(ImGuiCol_Button, c);
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, c);
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, c);
				}
				if (ImGui::SmallButton(label))
					st.metric = m;
				if (on)
					ImGui::PopStyleColor(3);
			};
		seg("Value", GnMetric::Value);  ImGui::SameLine(0.f, 2.f);
		seg("Gene", GnMetric::Gene);    ImGui::SameLine(0.f, 2.f);
		seg("Sigma", GnMetric::Sigma);

		ImGui::SameLine(0.f, 14.f);
		ImGui::Checkbox("Sort by spread", &st.sort_by_spread);
		ImGui::SameLine();
		ImGui::Checkbox("Hide converged", &st.hide_converged);
		ImGui::SameLine();
		ImGui::Checkbox("Flags", &st.show_flags);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(110.f);
		ImGui::InputTextWithHint("##gn_filter", "filter genes", st.filter, sizeof st.filter);
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");

		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::Text("Value = real units   Gene = raw 0..1 gene value   Sigma = per-gene mutation strength");
			ImGui::Text("Bar = position inside the gene's limits (Sigma view: log position inside the sigma range)");
			ImGui::Text("Spread = how much the organism's cells differ for this trait (0 = identical)");
			ImGui::TextColored(k_orange, "Orange edge = gene sits at one of its limits");
			ImGui::TextColored(k_accent, "Small triangle = outside the generation-0 spawn range (up = above, down = below)");
			ImGui::TextDisabled("Clicking a cell or header selects it here and in Cells & Springs");
			ImGui::EndTooltip();
		}
	}

	void draw_summary(const GnCtx& g)
	{
		const OrganismTracker& t = g.t;
		const int nc = CellKind::count(t);
		const int ns = TendonKind::count(t);
		if (nc == 0)
		{
			ImGui::TextDisabled("No cells");
			return;
		}

		uint32_t gen_min = UINT32_MAX, gen_max = 0;
		float quorum_sum = 0.f, sigma_cells = 0.f, sigma_tendons = 0.f;
		int ready = 0;
		for (int i = 0; i < nc; ++i)
		{
			const Cell& c = t.cells[i];
			gen_min = std::min(gen_min, c.generation());
			gen_max = std::max(gen_max, c.generation());
			quorum_sum += c.get<CellGenome::QuorumFraction>();
			ready += c.should_reproduce() ? 1 : 0;
			sigma_cells += c.mean_mutation_strength(CellGenome::SharedCount, CellGenome::gene_count);
		}
		for (int i = 0; i < ns; ++i)
			sigma_tendons += t.springs[i].genome.mean_mutation_strength(TendonGenome::SharedCount, TendonGenome::gene_count);

		// same rule as CellManager::can_protozoa_reproduce
		const float quorum = quorum_sum / static_cast<float>(nc);
		const int needed = std::max(1, static_cast<int>(std::ceil(quorum * static_cast<float>(nc))));

		ImGui::TextDisabled("Cells %d  Springs %d   Gen %u-%u   Sigma: cells %.4f  springs %.4f   Quorum: %d of %d cells must be ready (%d now)",
			nc, ns, gen_min, gen_max, sigma_cells / static_cast<float>(nc),
			ns > 0 ? sigma_tendons / static_cast<float>(ns) : 0.f, needed, nc, ready);
	}

	// ── The matrix ───────────────────────────────────────────────────────────
	template <class K>
	void draw_table(GnCtx& g, const char* child_id, const char* table_id, const char* title, const float width)
	{
		GenesTabState& st = g.st;
		const OrganismTracker& t = g.t;
		const auto& infos = K::info();
		const std::vector<int>& order = K::order(g);
		const int n = entity_count<K>(g);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.f, 2.f));
		ImGui::BeginChild(child_id, ImVec2(width, -1.f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		ImGui::PopStyleVar();

		if (n == 0)
		{
			ImGui::TextDisabled("No %s", K::plural);
			ImGui::EndChild();
			return;
		}

		std::array<RowStats, k_max_genes> stats{};
		for (size_t i = 0; i < infos.size(); ++i)
			stats[i] = compute_stats<K>(g, i, n);

		const bool filter_active = st.filter[0] != '\0';

		constexpr ImGuiTableFlags flags = ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV
			| ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_SizingFixedFit;

		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(3.f, 1.f));
		if (ImGui::BeginTable(table_id, n + 3, flags))
		{
			ImGui::TableSetupColumn("Gene", ImGuiTableColumnFlags_WidthFixed, k_name_w);
			for (int k = 0; k < n; ++k)
			{
				char nm[8];
				std::snprintf(nm, sizeof nm, "%c%d", K::prefix, k);
				ImGui::TableSetupColumn(nm, ImGuiTableColumnFlags_WidthFixed, k_col_w);
			}
			ImGui::TableSetupColumn("Mean", ImGuiTableColumnFlags_WidthFixed, k_mean_w);
			ImGui::TableSetupColumn("Spread", ImGuiTableColumnFlags_WidthFixed, k_spread_w);
			ImGui::TableSetupScrollFreeze(1, 1);

			ImDrawList* dl = ImGui::GetWindowDrawList();

			// ── Header row: title | chip + id per entity | Mean | Spread ──
			ImGui::TableNextRow(ImGuiTableRowFlags_Headers, k_row_h + 2.f);

			const auto header_text = [&](const char* text, const ImU32 col)
				{
					const ImVec2 p0 = ImGui::GetCursorScreenPos();
					const float w = ImGui::GetContentRegionAvail().x;
					const ImVec2 ts = ImGui::CalcTextSize(text);
					dl->AddText({ p0.x + 2.f, p0.y + (k_row_h - ts.y) * 0.5f + 1.f }, col, text);
					ImGui::Dummy({ w, k_row_h });
				};

			ImGui::TableSetColumnIndex(0);
			{
				char ttl[40];
				std::snprintf(ttl, sizeof ttl, "%s  (%d)", title, n);
				header_text(ttl, IM_COL32(150, 185, 255, 255));
			}

			for (int k = 0; k < n; ++k)
			{
				const int idx = order[k];
				ImGui::TableSetColumnIndex(1 + k);
				ImGui::PushID(k);

				const ImVec2 p0 = ImGui::GetCursorScreenPos();
				const float w = ImGui::GetContentRegionAvail().x;
				const ImVec2 p1{ p0.x + w, p0.y + k_row_h };
				const bool clicked = ImGui::InvisibleButton("##h", { w, k_row_h });
				const bool hovered = ImGui::IsItemHovered();

				if (K::selected(g, idx))
					dl->AddRectFilled(p0, p1, IM_COL32(70, 90, 150, 110));
				else if (hovered)
					dl->AddRectFilled(p0, p1, IM_COL32(255, 255, 255, 18));

				draw_chip(dl, { p0.x + 4.f, p0.y + (k_row_h - 11.f) * 0.5f }, 11.f, K::outer(t, idx), K::inner(t, idx));

				char lbl[8];
				std::snprintf(lbl, sizeof lbl, "%c%d", K::prefix, k);
				const ImVec2 ts = ImGui::CalcTextSize(lbl);
				dl->AddText({ p0.x + 19.f, p0.y + (k_row_h - ts.y) * 0.5f }, IM_COL32(200, 205, 220, 255), lbl);

				if (clicked)
					K::select(g, idx);
				if (hovered)
					entity_tooltip<K>(g, idx, k, n);
				ImGui::PopID();
			}

			ImGui::TableSetColumnIndex(1 + n);
			header_text("Mean", IM_COL32(150, 156, 175, 255));
			ImGui::TableSetColumnIndex(2 + n);
			header_text("Spread", IM_COL32(150, 156, 175, 255));

			// ── Gene rows, grouped ──
			struct RowRef { int gene; float spread; };
			bool drawn_any = false;

			for (int gi = 0; gi < static_cast<int>(GeneGroup::Count); ++gi)
			{
				std::array<RowRef, k_max_genes> rows{};
				int nr = 0;
				for (size_t i = 0; i < infos.size(); ++i)
				{
					if (static_cast<int>(infos[i].group) != gi)
						continue;
					if (filter_active && !icontains(infos[i].name, st.filter))
						continue;
					if (st.hide_converged&& n >= 2 && stats[i].spread < k_converged)
						continue;
					rows[nr++] = { static_cast<int>(i), stats[i].spread };
				}
				if (nr == 0)
					continue;
				drawn_any = true;

				if (st.sort_by_spread)
					std::stable_sort(rows.begin(), rows.begin() + nr, [](const RowRef& a, const RowRef& b) { return a.spread > b.spread; });

				// group header: click to fold / unfold (a filter forces every group open)
				const bool closed = !filter_active && ((st.closed_groups >> gi) & 1u);
				ImGui::TableNextRow(ImGuiTableRowFlags_None, k_row_h);
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(34, 37, 46, 255));
				ImGui::TableSetColumnIndex(0);
				ImGui::PushID(1000 + gi);
				{
					const ImVec2 p0 = ImGui::GetCursorScreenPos();
					char lbl[48];
					std::snprintf(lbl, sizeof lbl, "     %s", gene_group_name(static_cast<GeneGroup>(gi)));
					if (ImGui::Selectable(lbl, false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, k_cell_h)))
						st.closed_groups ^= (1u << gi);
					draw_fold_arrow(dl, { p0.x + 4.f, p0.y + 4.f }, !closed);
				}
				ImGui::PopID();
				if (closed)
					continue;

				for (int r = 0; r < nr; ++r)
				{
					const size_t gene = static_cast<size_t>(rows[r].gene);
					const GeneInfo& info = infos[gene];
					const RowStats& rs = stats[gene];
					const bool row_sel = st.sel_is_tendon == K::is_tendon && st.sel_gene == static_cast<int>(gene);

					ImGui::TableNextRow(ImGuiTableRowFlags_None, k_row_h);
					ImGui::PushID(static_cast<int>(gene));

					// name
					ImGui::TableSetColumnIndex(0);
					ImGui::Indent(8.f);
					if (ImGui::Selectable(info.name, row_sel, ImGuiSelectableFlags_None, ImVec2(0.f, k_cell_h)))
					{
						st.sel_is_tendon = K::is_tendon;
						st.sel_gene = static_cast<int>(gene);
					}
					ImGui::Unindent(8.f);
					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::PushTextWrapPos(320.f);
						ImGui::TextColored(k_accent, "%s", info.name);
						ImGui::TextWrapped("%s", info.tip);
						ImGui::PopTextWrapPos();
						ImGui::EndTooltip();
					}

					// one cell per entity
					for (int k = 0; k < n; ++k)
					{
						const int idx = order[k];
						ImGui::TableSetColumnIndex(1 + k);

						const Sample s = read<K>(t, idx, gene);
						char txt[24];
						format_value(txt, sizeof txt, info, st.metric, s.pheno, s.norm, s.sigma);

						const bool col_sel = K::selected(g, idx);
						const ImVec2 p0 = ImGui::GetCursorScreenPos();
						const float w = ImGui::GetContentRegionAvail().x;
						const ImVec2 p1{ p0.x + w, p0.y + k_cell_h };

						ImGui::PushID(k);
						const bool clicked = ImGui::InvisibleButton("##v", { w, k_cell_h });
						const bool hovered = ImGui::IsItemHovered();
						ImGui::PopID();

						paint_cell(dl, p0, p1, slot_bg(row_sel, col_sel), metric_pos(s, st.metric),
							bright32(K::outer(t, idx), 85), txt, row_sel && col_sel);

						if (st.show_flags)
						{
							const GeneFlags f = flags_for<K>(t, idx, gene, s.norm);
							const ImU32 edge = IM_COL32(255, 175, 60, 255);
							if (f.at_min) dl->AddRectFilled(p0, { p0.x + 2.f, p1.y }, edge);
							if (f.at_max) dl->AddRectFilled({ p1.x - 2.f, p0.y }, p1, edge);

							const ImU32 mark = IM_COL32(90, 200, 235, 255);
							if (f.above_spawn)
								dl->AddTriangleFilled({ p0.x + 4.f, p0.y + 7.f }, { p0.x + 10.f, p0.y + 7.f }, { p0.x + 7.f, p0.y + 3.f }, mark);
							else if (f.below_spawn)
								dl->AddTriangleFilled({ p0.x + 4.f, p0.y + 3.f }, { p0.x + 10.f, p0.y + 3.f }, { p0.x + 7.f, p0.y + 7.f }, mark);
						}

						if (clicked)
						{
							K::select(g, idx);
							st.sel_is_tendon = K::is_tendon;
							st.sel_gene = static_cast<int>(gene);
						}
						if (hovered)
							gene_tooltip<K>(g, idx, k, gene, s);
					}

					// Mean and Spread
					const auto stat_cell = [&](const int col, const float pos, const ImU32 fill, const char* text)
						{
							ImGui::TableSetColumnIndex(col);
							const ImVec2 p0 = ImGui::GetCursorScreenPos();
							const float w = ImGui::GetContentRegionAvail().x;
							ImGui::PushID(col);
							const bool clicked = ImGui::InvisibleButton("##s", { w, k_cell_h });
							ImGui::PopID();
							paint_cell(dl, p0, { p0.x + w, p0.y + k_cell_h }, slot_bg(row_sel, false), pos, fill, text, false);
							if (clicked)
							{
								st.sel_is_tendon = K::is_tendon;
								st.sel_gene = static_cast<int>(gene);
							}
						};

					char mean_txt[24];
					format_value(mean_txt, sizeof mean_txt, info, st.metric, rs.mean_pheno, rs.mean_norm, rs.mean_sigma);
					stat_cell(1 + n, rs.mean_pos, IM_COL32(150, 160, 190, 55), mean_txt);

					char spread_txt[16];
					if (n >= 2) std::snprintf(spread_txt, sizeof spread_txt, "%.2f", rs.spread);
					else        std::snprintf(spread_txt, sizeof spread_txt, "-");
					stat_cell(2 + n, n >= 2 ? rs.spread : 0.f, spread_fill(rs.spread), spread_txt);

					ImGui::PopID();
				}
			}

			if (!drawn_any)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextDisabled("no genes match");
			}

			ImGui::EndTable();
		}
		ImGui::PopStyleVar();
		ImGui::EndChild();
	}

	// ── Inspector pieces ─────────────────────────────────────────────────────

	// Every entity's value for one gene as a dot on its limits, with the spawn band and the organism mean
	template <class K>
	void draw_strip(GnCtx& g, const size_t gene, const int n, const RowStats& rs)
	{
		const auto& order = K::order(g);
		const Range init = K::genome(g.t, order[0]).gene_initial_range(gene);

		const float w = ImGui::GetContentRegionAvail().x;
		const ImVec2 p0 = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("##gn_strip", { w, k_strip_h });
		const bool hovered = ImGui::IsItemHovered();
		const bool clicked = ImGui::IsItemClicked();
		ImDrawList* dl = ImGui::GetWindowDrawList();

		const float x0 = p0.x + 8.f, x1 = p0.x + w - 8.f, L = x1 - x0;
		const float cy = p0.y + k_strip_h * 0.5f;

		dl->AddRectFilled(p0, { p0.x + w, p0.y + k_strip_h }, IM_COL32(22, 24, 30, 255), 3.f);

		float bx0 = x0 + init.min * L, bx1 = x0 + init.max * L;
		if (bx1 - bx0 < 2.f) bx1 = bx0 + 2.f;
		dl->AddRectFilled({ bx0, p0.y + 3.f }, { bx1, p0.y + k_strip_h - 3.f }, IM_COL32(70, 130, 190, 55));

		dl->AddLine({ x0, cy }, { x1, cy }, IM_COL32(90, 96, 112, 255), 1.f);
		dl->AddLine({ x0, cy - 5.f }, { x0, cy + 5.f }, IM_COL32(90, 96, 112, 255), 1.f);
		dl->AddLine({ x1, cy - 5.f }, { x1, cy + 5.f }, IM_COL32(90, 96, 112, 255), 1.f);

		const float mx = x0 + rs.mean_norm * L;
		dl->AddLine({ mx, p0.y + 3.f }, { mx, p0.y + k_strip_h - 3.f }, IM_COL32(255, 255, 255, 110), 1.f);

		std::array<float, k_max_entities> xs{};
		const float mouse_x = ImGui::GetIO().MousePos.x;
		int nearest = -1;
		float nearest_d = 1e9f;

		for (int k = 0; k < n; ++k)
		{
			const int idx = order[k];
			const Sample s = read<K>(g.t, idx, gene);
			const float x = x0 + s.norm * L;
			xs[k] = x;

			int lane = 0;                       // nudge dots that would sit on top of each other
			for (int j = 0; j < k; ++j)
				if (std::abs(xs[j] - x) < 7.f)
					++lane;
			const float y = cy + (static_cast<float>(lane % 3) - 1.f) * 9.f;

			dl->AddCircleFilled({ x, y }, 4.f, bright32(K::outer(g.t, idx), 255));
			dl->AddCircle({ x, y }, 4.f, IM_COL32(0, 0, 0, 160));
			if (idx == K::target(g))
				dl->AddCircle({ x, y }, 6.5f, IM_COL32(255, 255, 255, 230), 0, 1.5f);

			const float d = std::abs(mouse_x - x);
			if (d < nearest_d) { nearest_d = d; nearest = k; }
		}

		if (hovered && nearest >= 0 && nearest_d < 12.f)
		{
			const Sample s = read<K>(g.t, order[nearest], gene);
			ImGui::SetTooltip("%s %d : %.5f", K::noun, nearest, s.pheno);
			if (clicked)
				K::select(g, order[nearest]);
		}
	}

	// Every entity's wave for the next N frames, driven by the live phase so tendon sync shows up
	template <class K>
	void draw_wave_overlay(GnCtx& g, const int n)
	{
		GenesTabState& st = g.st;
		static double xs[k_wave_samples], ys[k_wave_samples];
		const auto& order = K::order(g);
		const double window = static_cast<double>(st.wave_window);
		for (int i = 0; i < k_wave_samples; ++i)
			xs[i] = window * static_cast<double>(i) / static_cast<double>(k_wave_samples - 1);

		ImPlot::PushStyleColor(ImPlotCol_FrameBg, { 0.f, 0.f, 0.f, 0.f });
		ImPlot::PushStyleColor(ImPlotCol_PlotBg, { 0.f, 0.f, 0.f, 0.f });
		ImPlot::PushStyleColor(ImPlotCol_PlotBorder, { 0.f, 0.f, 0.f, 0.f });
		ImPlot::PushStyleColor(ImPlotCol_AxisGrid, { 1.f, 1.f, 1.f, 0.07f });
		ImPlot::PushStyleColor(ImPlotCol_AxisText, { 0.55f, 0.58f, 0.65f, 1.f });
		ImPlot::PushStyleVar(ImPlotStyleVar_PlotBorderSize, 0.f);

		constexpr ImPlotFlags pf = ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect |
			ImPlotFlags_NoMouseText | ImPlotFlags_NoLegend | ImPlotFlags_NoInputs;

		if (ImPlot::BeginPlot("##gn_wave", { -1.f, 70.f }, pf))
		{
			static const double y_ticks[] = { 0.0, 0.5, 1.0 };
			ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoTickMarks, ImPlotAxisFlags_NoTickMarks);
			ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, window, ImGuiCond_Always);
			ImPlot::SetupAxisLimits(ImAxis_Y1, -0.06, 1.06, ImGuiCond_Always);
			ImPlot::SetupAxisTicks(ImAxis_Y1, y_ticks, 3);

			for (int pass = 0; pass < 2; ++pass)           // selected entity last so it sits on top
			{
				for (int k = 0; k < n; ++k)
				{
					const int idx = order[k];
					const bool sel = idx == K::target(g);
					if (sel != (pass == 1))
						continue;

					const auto& gn = K::genome(g.t, idx);
					const float phase = K::phase(g.t, idx);
					for (int i = 0; i < k_wave_samples; ++i)
						ys[i] = static_cast<double>(K::wave_level(gn, phase, static_cast<float>(xs[i])));

					char lbl[16];
					std::snprintf(lbl, sizeof lbl, "##w%d", k);
					ImPlot::SetNextLineStyle(bright4(K::outer(g.t, idx), sel ? 1.f : 0.55f), sel ? 2.5f : 1.2f);
					ImPlot::PlotLine(lbl, xs, ys, k_wave_samples);
				}
			}
			ImPlot::EndPlot();
		}
		ImPlot::PopStyleVar(1);
		ImPlot::PopStyleColor(5);

		ImGui::SetNextItemWidth(-1.f);
		ImGui::SliderInt("##gn_win", &st.wave_window, 60, 600, "Next %d frames (live phase)");
	}

	template <class K>
	void draw_inspector_body(GnCtx& g)
	{
		GenesTabState& st = g.st;
		const OrganismTracker& t = g.t;
		const auto& infos = K::info();
		const int n = entity_count<K>(g);
		const size_t gene = static_cast<size_t>(st.sel_gene);

		if (n <= 0 || gene >= infos.size())
		{
			ImGui::TextDisabled("No %s in this organism.", K::plural);
			return;
		}

		const GeneInfo& info = infos[gene];
		const auto& order = K::order(g);
		const int target = std::clamp(K::target(g), 0, K::count(t) - 1);
		const int rank = K::rank(g)[static_cast<size_t>(target)];
		const auto& gn = K::genome(t, target);
		const RowStats rs = compute_stats<K>(g, gene, n);
		const Sample s = read<K>(t, target, gene);
		const Range lim = gn.limits(gene);
		const Range init = gn.gene_initial_range(gene);
		const float span = lim.max - lim.min;

		// title
		ImGui::TextColored(k_accent, "%s", info.name);
		ImGui::SameLine();
		ImGui::TextDisabled("%s  |  %s %d", gene_group_name(info.group), K::noun, rank);

		draw_strip<K>(g, gene, n, rs);

		// numbers for the selected entity
		ImGui::Text("Value  %.5f", s.pheno);
		ImGui::SameLine(0.f, 14.f);
		ImGui::TextDisabled("gene %.3f   sigma %.4f", s.norm, s.sigma);

		char d[64];
		if (derived_text(info, s.pheno, d, sizeof d))
			ImGui::TextColored(k_derived, "%s", d);

		ImGui::TextDisabled("Limits %.4g .. %.4g   Spawn %.4g .. %.4g",
			lim.min, lim.max, lim.min + init.min * span, lim.min + init.max * span);
		if (n >= 2)
			ImGui::TextDisabled("Organism mean %.4g   spread %.2f", rs.mean_pheno, rs.spread);
		tooltip_flags(flags_for<K>(t, target, gene, s.norm));

		// live edit
		if (info.editable)
		{
			ImGui::Separator();
			float v = s.pheno;
			ImGui::SetNextItemWidth(-64.f);
			if (ImGui::SliderFloat("##gn_edit", &v, lim.min, lim.max, "%.4f"))
				push_gene_cmd<K>(g, K::id(t, target), gene, v);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Edits this %s only. Ctrl+click to type a value.", K::noun);
			ImGui::SameLine();
			if (ImGui::Button("Set all"))
			{
				for (int k = 0; k < n; ++k)
					push_gene_cmd<K>(g, K::id(t, order[k]), gene, s.pheno);
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Copy this value to every %s of the organism", K::plural);
		}

		if (info.group == K::wave_group)
		{
			ImGui::Separator();
			draw_wave_overlay<K>(g, n);
		}

		ImGui::Separator();
		ImGui::PushTextWrapPos(0.f);
		ImGui::TextDisabled("%s", info.tip);
		ImGui::PopTextWrapPos();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Top-level draw
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_genes_tab(const SimSnapshot& /*snap*/, ImGuiContext& ctx, const OrganismTracker& protozoa)
{
	GenesTabState& st = m_genes_;
	if (!st.initialised)
	{
		st.closed_groups = 1u << static_cast<unsigned>(GeneGroup::Colour);   // colours start folded
		st.initialised = true;
	}

	GnCtx g{ st, ctx, protozoa, m_sel_cell_idx_, m_sel_spring_idx_, m_sel_is_spring_,
		m_cell_order_, m_cell_rank_, m_spring_order_, m_spring_rank_, m_sorted_cell_keys_ };

	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	const float main_w = std::max(ImGui::GetContentRegionAvail().x - k_inspector_w - spacing, 320.f);

	ImGui::BeginChild("GN_main", ImVec2(main_w, -1.f), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	draw_toolbar(st);
	draw_summary(g);

	// Each table asks for the width its columns need. Spare room is shared out in proportion,
	// and when there is not enough room the split is proportional and the tables scroll sideways.
	const int nc = entity_count<CellKind>(g);
	const int ns = entity_count<TendonKind>(g);
	const float total = ImGui::GetContentRegionAvail().x - spacing;
	const auto want = [](const int n)
		{
			return n > 0 ? k_name_w + static_cast<float>(n) * k_col_w + k_mean_w + k_spread_w + k_table_extra : 200.f;
		};
	const float d_c = want(nc), d_s = want(ns);

	float w_c;
	if (d_c + d_s <= total)
		w_c = d_c + (total - d_c - d_s) * (d_c / (d_c + d_s));
	else
		w_c = std::clamp(total * d_c / (d_c + d_s), k_min_table_w, std::max(k_min_table_w, total - k_min_table_w));
	const float w_s = total - w_c;

	draw_table<CellKind>(g, "GN_cells_child", "GN_cells", "CELLS", w_c);
	ImGui::SameLine();
	draw_table<TendonKind>(g, "GN_springs_child", "GN_springs", "SPRINGS", w_s);
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("GN_inspector", ImVec2(k_inspector_w, -1.f), true);
	if (st.sel_is_tendon) draw_inspector_body<TendonKind>(g);
	else                  draw_inspector_body<CellKind>(g);
	ImGui::EndChild();
}