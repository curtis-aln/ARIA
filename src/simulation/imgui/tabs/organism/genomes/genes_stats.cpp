// Genes tab: per-gene statistics across the organism, and limit / spawn-range flags.
#include "genes_ui_common.h"

#include <entities/cell/cell_settings.h>

namespace genes_ui
{
	float sigma_pos(const float sigma)
	{
		const float lo = std::log(GeneSettings::mutation_strength_range.min);
		const float hi = std::log(GeneSettings::mutation_strength_range.max);
		return std::clamp((std::log(std::max(sigma, 1e-6f)) - lo) / (hi - lo), 0.f, 1.f);
	}

	// Mean and spread of one gene across the first `entity_total` entities.
	// Genes that wrap around (angles, phases) use circular statistics, so 0.99 and 0.01 count as close.
	template <class K>
	RowStats compute_stats(const GnCtx& g, const size_t gene, const int entity_total)
	{
		RowStats stats;
		if (entity_total <= 0)
			return stats;

		const auto& order = K::order(g);
		const auto& first_genome = K::genome(g.t, order[0]);
		const bool wraps = first_genome.gene_wraps(gene);
		const Range limits = first_genome.limits(gene);
		const GnMetric metric = g.st.metric;
		const bool circular = wraps && metric != GnMetric::Sigma;

		// Running sums over every entity
		float sum_pos = 0.f, sum_pos_sq = 0.f;                       // bar positions in the current metric
		float sum_pheno = 0.f, sum_sigma = 0.f, sum_norm = 0.f;
		float sum_cos = 0.f, sum_sin = 0.f;                          // unit vectors for the circular mean
		for (int k = 0; k < entity_total; ++k)
		{
			const Sample s = read<K>(g.t, order[k], gene);
			const float pos = metric_pos(s, metric);
			sum_pos += pos;
			sum_pos_sq += pos * pos;
			sum_pheno += s.pheno;
			sum_sigma += s.sigma;
			sum_norm += s.norm;
			sum_cos += std::cos(k_two_pi * s.norm);
			sum_sin += std::sin(k_two_pi * s.norm);
		}

		const float inv_count = 1.f / static_cast<float>(entity_total);
		stats.mean_sigma = sum_sigma * inv_count;

		if (wraps)
		{
			const float turns = std::atan2(sum_sin, sum_cos) / k_two_pi;
			stats.mean_norm = turns - std::floor(turns);
			stats.mean_pheno = limits.min + stats.mean_norm * (limits.max - limits.min);
		}
		else
		{
			stats.mean_norm = sum_norm * inv_count;
			stats.mean_pheno = sum_pheno * inv_count;
		}
		stats.mean_pos = (metric == GnMetric::Sigma) ? sigma_pos(stats.mean_sigma) : stats.mean_norm;

		if (entity_total >= 2)
		{
			if (circular)
			{
				// circular standard deviation, in turns, doubled so it lines up with the linear case
				const float resultant = std::clamp(std::sqrt(sum_cos * sum_cos + sum_sin * sum_sin) * inv_count, 1e-6f, 1.f);
				stats.spread = std::min(1.f, std::sqrt(-2.f * std::log(resultant)) / k_two_pi * 2.f);
			}
			else
			{
				const float mean = sum_pos * inv_count;
				const float variance = std::max(0.f, sum_pos_sq * inv_count - mean * mean);
				stats.spread = std::min(1.f, std::sqrt(variance) * 2.f);
			}
		}
		return stats;
	}

	template <class K>
	GeneFlags flags_for(const OrganismTracker& t, const int idx, const size_t gene, const float norm)
	{
		const auto& genome = K::genome(t, idx);
		GeneFlags flags;

		// Wrapping genes have no limits to sit against
		if (!genome.gene_wraps(gene))
		{
			flags.at_min = norm <= k_limit_eps;
			flags.at_max = norm >= 1.f - k_limit_eps;
		}

		const Range spawn = genome.gene_initial_range(gene);
		flags.above_spawn = norm > spawn.max + k_spawn_eps;
		flags.below_spawn = norm < spawn.min - k_spawn_eps;
		return flags;
	}

	template RowStats compute_stats<CellKind>(const GnCtx&, size_t, int);
	template RowStats compute_stats<TendonKind>(const GnCtx&, size_t, int);
	template GeneFlags flags_for<CellKind>(const OrganismTracker&, int, size_t, float);
	template GeneFlags flags_for<TendonKind>(const OrganismTracker&, int, size_t, float);
}