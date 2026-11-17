#pragma once

#include "helpers.h"
#include "gene.h"

// ---------------- Genome Base Class ---------------- //
// The GenomeBase class is a template class that represents the base genome for an organism. It contains a fixed number of shared genes (for color) and allows derived classes to add extra genes specific to their needs. The class provides functionality for randomization, mutation, crossover, and copying of genomes.

struct GeneDef
{
	Range limits;                // hard limits of the expressed trait (phenotype units)
	Range spawn = { 1.f, 0.f };  // where generation-0 values start; omit it to use all of `limits`
	bool  wrap = false;         // angles etc.

	constexpr Range initial() const { return spawn.min > spawn.max ? limits : spawn; }
};

inline Gene make_gene(const GeneDef& d)
{
	const Range s = d.initial();
	assert(d.limits.contains(s) && "spawn range must sit inside limits");

	const float span = d.limits.max - d.limits.min;
	const Range initial{ (s.min - d.limits.min) / span,
						 (s.max - d.limits.min) / span };
	return Gene{ d.limits, initial, d.wrap, true };
}

namespace GenomeBaseSettings
{
	inline constexpr uint8_t outer_transparency = 150;
	inline constexpr uint8_t inner_transparency = 125;
}

template <size_t ExtraGenes = 0>
class GenomeBase
{
public:
	// Shared gene indices. Derived classes continue numbering from SharedCount.
	enum SharedGene : size_t { OuterR, OuterG, OuterB, InnerR, InnerG, InnerB, SharedCount };

	// Total number of genes in the genome, including shared and extra genes.
	static constexpr size_t gene_count = SharedCount + ExtraGenes;

	// Setters
	void randomize() {
		for (auto& g : genes_)
			g.randomize();
		compile_phenotypes();
	}

	void mutate() {
		for (auto& g : genes_)
		{
			g.mutate();
			compile_phenotypes();
		}
		increment_generation();
	}

	void reset_generation() { generation_ = 0; }

	void increment_generation() { ++generation_; }

	// Getters
	uint32_t generation() const { return generation_; }

	// Crossover and Copy
	void crossover(const GenomeBase& other)
	{
		for (size_t i = 0; i < gene_count; ++i) genes_[i].crossover(other.genes_[i]);
		generation_ = std::max(generation_, other.generation_) + 1;
		compile_phenotypes();
	}

	void copy(const GenomeBase& other)
	{
		for (size_t i = 0; i < gene_count; ++i) genes_[i].copy(other.genes_[i]);
		generation_ = other.generation_;
		compile_phenotypes();
	}

	void sexually_reproduce(const GenomeBase& a, const GenomeBase& b) { copy(a); crossover(b); }

	// Color Accessors
	const sf::Color& outer_color() const { return outer_color_; }
	const sf::Color& inner_color() const { return inner_color_; }

	// Phenotype Accessors
	float phenotype(size_t i) const
	{
		assert(i < gene_count);
		return phenotypes_[i];
	}

	template <size_t I>
	float get() const
	{
		static_assert(I < gene_count, "gene index out of range");
		return phenotypes_[I];
	}

	template <size_t I>
	void set(float phenotype_value)
	{
		static_assert(I < gene_count, "gene index out of range");
		genes_[I].set_phenotype(phenotype_value);
		compile_phenotypes();
	}

	// Several at once, rebuilds the cache only once
	void set_phenotypes(std::initializer_list<std::pair<size_t, float>> values)
	{
		for (const auto& [index, value] : values)
		{
			assert(index < gene_count);
			genes_[index].set_phenotype(value);
		}
		compile_phenotypes();
	}

	Range limits(size_t i) const
	{
		assert(i < gene_count);
		return genes_[i].limits();
	}

	template <size_t I>
	Range limits() const
	{
		static_assert(I < gene_count, "gene index out of range");
		return genes_[I].limits();
	}

	// Generation-0 spawn range of a gene, normalised 0..1
	Range gene_initial_range(size_t i) const
	{
		assert(i < gene_count);
		return genes_[i].initial_range_;
	}

	// True if the gene wraps around at 0/1 (angles)
	bool gene_wraps(size_t i) const
	{
		assert(i < gene_count);
		return genes_[i].wrap_;
	}

	float gene_value(size_t i) const
	{
		assert(i < gene_count);
		return genes_[i].gene_val_;
	}

	float gene_mutation_strength(size_t i) const
	{
		assert(i < gene_count);
		return genes_[i].mutation_strength_;
	}

	// Average sigma across all genes: how fast this genome is currently evolving
	float mean_mutation_strength() const
	{
		float sum = 0.f;
		for (const auto& g : genes_)
			sum += g.mutation_strength_;
		return sum / static_cast<float>(gene_count);
	}

	// Same, but only over a gene index range [first, last), e.g. skip the colour genes
	float mean_mutation_strength(size_t first, size_t last) const
	{
		assert(first < last && last <= gene_count);
		float sum = 0.f;
		for (size_t i = first; i < last; ++i)
			sum += genes_[i].mutation_strength_;
		return sum / static_cast<float>(last - first);
	}

	// Mean per-gene distance to another genome, 0 = identical. Useful for diversity or relatedness
	float distance(const GenomeBase& other) const
	{
		float sum = 0.f;
		for (size_t i = 0; i < gene_count; ++i)
			sum += genes_[i].distance(other.genes_[i]);
		return sum / static_cast<float>(gene_count);
	}

protected:
	// Derived classes pass in the genes they add on top of the shared colour genes
	explicit GenomeBase(const std::array<Gene, ExtraGenes>& extra_genes = {})
	{
		constexpr Range rgb{ 0.f, 255.f };
		for (size_t i = 0; i < SharedCount; ++i)
			genes_[i] = Gene{ rgb,{ 0.f, 1.f }, false, true };

		std::copy(extra_genes.begin(), extra_genes.end(), genes_.begin() + SharedCount);

		compile_phenotypes();
	}

	// Derived classes that need to build extra cached values can call this at the end of their own constructor, see below
	void compile_phenotypes()
	{
		for (size_t i = 0; i < gene_count; ++i)
			phenotypes_[i] = genes_[i].phenotype();

		outer_color_ = sf::Color(static_cast<uint8_t>(phenotypes_[OuterR]),
			static_cast<uint8_t>(phenotypes_[OuterG]),
			static_cast<uint8_t>(phenotypes_[OuterB]), GenomeBaseSettings::outer_transparency);
		inner_color_ = sf::Color(static_cast<uint8_t>(phenotypes_[InnerR]),
			static_cast<uint8_t>(phenotypes_[InnerG]),
			static_cast<uint8_t>(phenotypes_[InnerB]), GenomeBaseSettings::inner_transparency);
	}

	// The genes are stored in a fixed-size array, with the first SharedCount genes reserved for shared traits (like color) and the rest for extra traits defined by derived classes.
	std::array<Gene, gene_count> genes_;
	std::array<float, gene_count> phenotypes_{};
	sf::Color outer_color_, inner_color_;

	uint32_t generation_ = 0;
};
