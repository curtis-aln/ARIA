#pragma once


#include "helpers.h"
#include <SFML/Graphics/Color.hpp>


// ---------------- Gene Class ---------------- //
namespace GeneSettings
{
	// The range of the mutation strength, how much the gene can change during mutation
	inline constexpr Range mutation_strength_range = { 0.01f, .2f };

	// Mutation strength (sigma) is mutated multiplicatively (log-normal) rather than additively.
	// Additive noise is a fixed size, so it is too violent for small sigma (can push it to 0)
	// and too gentle for large sigma. Multiplying by exp(tau * N(0,1)) changes sigma by a
	// percentage (~tau) at any scale, and exp() keeps it positive, so it can never reach 0.
	inline constexpr float tau = 0.2f;
}

struct Gene
{
	// variables
	float gene_val_ = 0.f; // the value of the gene between 0 and 1
	float mutation_strength_ = GeneSettings::mutation_strength_range.rand(); // the strength of the mutation, how much the gene can change during mutation

	Range phenotype_range_; // the real range of the expressed trait, a gene for radius will have a different range than a gene for frequency, for example
	Range initial_range_; // the initial random range of the gene value

	bool wrap_; // whether the gene value wraps around or not

	Gene(const Range& phenotype_range = { 0.f, 1.f }, const Range& initial_range = { 0.f, 1.f }, const bool wrap = false, const bool randomize_gene = true)
		: initial_range_(initial_range), phenotype_range_(phenotype_range), wrap_(wrap)
	{
		assert(initial_range.min >= 0 && initial_range.max <= 1 && initial_range.min <= initial_range.max);

		if (randomize_gene)
			randomize();
	}

	// processes
	void mutate()
	{
		// update sigma first, then use the new one (Schwefel-style)
		mutate_mutation_strength();

		gene_val_ += sample_gaussian() * mutation_strength_;
		gene_val_ = wrap_ ? wrap01(gene_val_) : reflect01(gene_val_);
	}

	void copy(const Gene& other)
	{
		gene_val_ = other.gene_val_;
		mutation_strength_ = other.mutation_strength_;

		phenotype_range_ = other.phenotype_range_;
		initial_range_ = other.initial_range_;

		wrap_ = other.wrap_;
	}

	void crossover(const Gene& other)
	{
		// For this simulation it makes more sense to average the two genes, rather than randomly picking one of the two. This is because springs are made between two cells and their genome is the combination of the two cell's genomes. if we choose random genes the sprigns behaviour will be different every single time and the organism as a whole will struggle to survive.
		if (wrap_)
		{
			// circular mean of two points on [0,1)
			float d = other.gene_val_ - gene_val_;
			if (d > 0.5f) d -= 1.f;
			if (d < -0.5f) d += 1.f;
			gene_val_ = wrap01(gene_val_ + 0.5f * d);
		}
		else
		{
			gene_val_ = (gene_val_ + other.gene_val_) * 0.5f;
		}
		mutation_strength_ = (mutation_strength_ + other.mutation_strength_) * 0.5f;
	}

	void randomize()
	{
		gene_val_ = initial_range_.rand();
		mutation_strength_ = GeneSettings::mutation_strength_range.rand();
	}

	float phenotype() const
	{
		return phenotype_range_.min + gene_val_ * (phenotype_range_.max - phenotype_range_.min);
	}

private:
	void mutate_mutation_strength()
	{
		const float lo = std::log(GeneSettings::mutation_strength_range.min);
		const float hi = std::log(GeneSettings::mutation_strength_range.max);
		float t = (std::log(mutation_strength_) + GeneSettings::tau * sample_gaussian() - lo) / (hi - lo);
		mutation_strength_ = std::exp(lo + reflect01(t) * (hi - lo));
	}
};
