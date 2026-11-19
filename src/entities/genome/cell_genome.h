#pragma once

#include "genome_base.h"
#include "../body/body.h"


inline constexpr size_t CellExtraGenes = 19;

class CellGenome : public GenomeBase<CellExtraGenes>
{
public:
	enum CellGene : size_t
	{
		Amplitude = SharedCount,
		Frequency, 
		Offset, 
		VerticalShift,
		BirthEnergyThresh, 
		BirthIntegrityThresh, 
		BirthNutrientsThresh, 
		OffspringEnergyToGive,
		ConnectiveTendonSpringConst, 
		ConnectiveTendonDamping,
		AddCellChance, 
		RemoveCellChance, 
		AddTendonChance, 
		RemoveTendonChance,
		NewbornSearchRadius, 
		QuorumFraction,
		Mass,
		InitialGrowthRate,
		GrowthDecay,
		CellGeneEnd
	};
	static_assert(CellGeneEnd == gene_count, "CellGene enum and CellExtraGenes are out of sync");

	CellGenome() : GenomeBase<CellExtraGenes>(make_genes()) {}

	// Mass is the only gene that drives body state. Call this after construction,
	// mutate, copy, crossover or sexually_reproduce.
	void apply_to_body(Body* body) const
	{
		body->set_mass(get<Mass>());
		body->set_radius();
	}

private:
	// Must stay in the same order as the CellGene enum
	static std::array<Gene, CellExtraGenes> make_genes()
	{
		return {
			// Drag-level wave: level = clamp(vertical_shift + amplitude * sin(frequency * t + offset), 0, 1)
			make_gene({ .limits = { 0.f, 1.f },               .spawn = { 0.05f, 0.9f } }),            // Amplitude
			make_gene({ .limits = { 1.f / 240.f, 1.f / 3.f }, .spawn = { 1.f / 20.f, 1.f / 5.f } }),  // Frequency
			make_gene({ .limits = { -3.14159f, 3.14159f },    .wrap = true }),                        // Offset
			make_gene({ .limits = { 0.f, 1.f },               .spawn = { 0.3f, 0.7f } }),             // VerticalShift

			// Reproduction thresholds
			make_gene({ .limits = { 0.f, 1.f },               .spawn = { 0.8f, 0.85f } }),            // BirthEnergyThresh
			make_gene({ .limits = { 0.f, 1.f },               .spawn = { 0.f, 0.1f } }),              // BirthIntegrityThresh
			make_gene({ .limits = { 0.f, 1.f },               .spawn = { 0.f, 0.1f } }),              // BirthNutrientsThresh
			make_gene({ .limits = { 0.f, 1.f } }),                                                    // OffspringEnergyToGive

			// Tendons this cell creates when it reproduces (same limits as TendonGenome)
			make_gene({ .limits = { 0.f, 5.f },               .spawn = { 1.55f, 2.8f } }),            // ConnectiveTendonSpringConst
			make_gene({ .limits = { 0.f, 1.f },               .spawn = { 0.01f, 0.1f } }),            // ConnectiveTendonDamping

			// Structural mutation chances
			make_gene({ .limits = { 0.f, 1.f },               .spawn = { 0.05f, 0.05f } }),           // AddCellChance
			make_gene({ .limits = { 0.f, 1.f },               .spawn = { 0.05f, 0.05f } }),           // RemoveCellChance
			make_gene({ .limits = { 0.f, 1.f },               .spawn = { 0.05f, 0.05f } }),           // AddTendonChance
			make_gene({ .limits = { 0.f, 1.f },               .spawn = { 0.05f, 0.05f } }),           // RemoveTendonChance

			make_gene({ .limits = { 0.f, 10 },                .spawn = { 0.20f, 0.80f } }),           // NewbornSearchRadius
			make_gene({ .limits = { 0.2f, 1.f },              .spawn = { 0.50f, 0.50f } }),             // QuorumFraction
			
			// Mass genes
			make_gene({ .limits = { 25.f, 75.f },             .spawn = { 25.f, 75.f } }),           // Mass
			make_gene({ .limits = { 0.001f, 0.1f },           .spawn = { 0.01f, 0.4f } }),           // InitialGrowthRate
			make_gene({ .limits = { 0.f, .01f },               .spawn = { 0.00001f, 0.0002f } }),           // GrowthDecay
		};
	}
};