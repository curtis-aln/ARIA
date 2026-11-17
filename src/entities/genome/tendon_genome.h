#pragma once

// These includes are for the guassian distribution sampling

#include "gene.h"
#include "helpers.h"
#include "genome_base.h"

/*
This is a genome class which uses containers to hold the genes

genotype - the raw genetic information
phenotype - the expressed traits of the organism, derived from the genotype

*/


inline constexpr size_t TendonExtraGenes = 8;

class TendonGenome : public GenomeBase<TendonExtraGenes>
{
public:
	enum TendonGene : size_t
	{
		Amplitude = SharedCount, Frequency, Offset, VerticalShift,
		NutrientTransferRate, SpringConst, Damping, PhaseSyncRate,
		TendonGeneEnd
	};
	static_assert(TendonGeneEnd == gene_count, "TendonGene enum and TendonExtraGenes are out of sync");

	TendonGenome() : GenomeBase<TendonExtraGenes>(make_genes()) {}

private:
	static std::array<Gene, TendonExtraGenes> make_genes()
	{
		return {
			make_gene({ .limits = { 0.f, 1.f },            .spawn = { 0.1f, 0.8f } }),            // Amplitude
			make_gene({ .limits = { -1.f / 120.f, 1.f },   .spawn = { 1.f / 15.f, 1.f / 5.f } }), // Frequency (cycles/frame)
			make_gene({ .limits = { -3.14159f, 3.14159f }, .wrap = true }),                       // Offset
			make_gene({ .limits = { -1.f, 1.f },           .spawn = { 0.3f, 1.f } }),             // VerticalShift
			make_gene({ .limits = { -1.f, 1.f },           .spawn = { -0.1f, 0.5f } }),           // NutrientTransferRate
			make_gene({ .limits = { 0.f, 5.f },            .spawn = { 1.55f, 2.8f } }),           // SpringConst
			make_gene({ .limits = { 0.f, 1.f },            .spawn = { 0.01f, 0.1f } }),           // Damping
			make_gene({ .limits = { 0.f, 1.f },            .spawn = { 0.f, 0.1f } }),             // PhaseSyncRate
		};
	}
};