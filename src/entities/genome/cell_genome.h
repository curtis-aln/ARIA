#pragma once

#include "genome_base.h"
#include "../body/body.h"


inline constexpr size_t CellExtraGenes = 17;

class CellGenome : public GenomeBase<CellExtraGenes>
{
public:
	enum CellGene : size_t
	{
		Mass = SharedCount, Amplitude, Frequency, Offset, VerticalShift,
		BirthEnergyThresh, BirthIntegrityThresh, BirthNutrientsThresh, OffspringEnergyToGive,
		ConnectiveTendonSpringConst, ConnectiveTendonDamping,
		AddCellChance, RemoveCellChance, AddTendonChance, RemoveTendonChance,
		NewbornSearchRadius, QuorumFraction,
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
		constexpr float mass_max = 135.f;
		constexpr float newborn_max = mass_max * 4.f;

		return {
			make_gene({ .limits = { 55.f, mass_max } }),                                              // Mass

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

			make_gene({ .limits = { 0.f, newborn_max },       .spawn = { newborn_max * 0.9f, newborn_max } }), // NewbornSearchRadius
			make_gene({ .limits = { 0.2f, 1.f },              .spawn = { 0.5f, 0.5f } }),             // QuorumFraction
		};
	}
};