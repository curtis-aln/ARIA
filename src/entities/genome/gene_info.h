#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "cell_genome.h"
#include "tendon_genome.h"

// UI metadata for every gene: display name, group, number format, tooltip, and whether the Genes tab
// may edit it. Tables are indexed by gene index (the CellGene / TendonGene enums). Every entry carries
// its enum value as `id`, and the static_asserts at the bottom stop the build if a table drifts out of
// order with its enum.
//
// Adding a gene: add it to the enum, to make_genes(), and add a row below in the SAME position.

// Declaration order is the order groups appear in the Genes tab.
enum class GeneGroup : uint8_t
{
	// cell genome
	Body, FrictionWave, Reproduction, Social, NewbornTendon, StructuralMutation,
	// tendon genome
	ExtensionWave, Nutrients, Mechanics, Sync,
	// both
	Colour,
	Count
};

constexpr const char* gene_group_name(const GeneGroup g)
{
	switch (g)
	{
	case GeneGroup::Body:               return "Body";
	case GeneGroup::FrictionWave:       return "Friction wave";
	case GeneGroup::Reproduction:       return "Reproduction";
	case GeneGroup::Social:             return "Social";
	case GeneGroup::NewbornTendon:      return "Newborn tendon";
	case GeneGroup::StructuralMutation: return "Structural mutation";
	case GeneGroup::ExtensionWave:      return "Extension wave";
	case GeneGroup::Nutrients:          return "Nutrients";
	case GeneGroup::Mechanics:          return "Mechanics";
	case GeneGroup::Sync:               return "Sync";
	case GeneGroup::Colour:             return "Colour";
	default:                            return "";
	}
}

// How the phenotype is printed in a matrix cell
enum class GeneFmt : uint8_t { Plain, Fine, Whole, Tenth };   // %.3f, %.4f, %.0f, %.1f

// Extra "what does this number mean" text shown in tooltips / the inspector
enum class GeneDerive : uint8_t
{
	None,
	Period,          // 2*pi / |f| frames
	EnergyAbs,       // fraction * CellSettings::max_energy
	IntegrityAbs,    // fraction * CellSettings::max_integrity
	NutrientsAbs,    // fraction * CellSettings::max_nutrients
	Percent,         // fraction as a percentage
	Degrees,         // radians as degrees
	EffectiveGive    // OffspringEnergyToGive after the sim's 0.1-0.9 clamp
};

struct GeneInfo
{
	size_t      id;
	const char* name;
	GeneGroup   group;
	GeneFmt     fmt;
	GeneDerive  derive;
	bool        editable;   // false = shown read-only in the inspector
	const char* tip;
};

namespace gene_info_detail
{
	static_assert(CellGenome::SharedCount == 6 && TendonGenome::SharedCount == 6, "shared colour gene count changed");

	inline constexpr std::array<GeneInfo, 6> shared_colour_info = { {
		{ 0, "Outer R", GeneGroup::Colour, GeneFmt::Whole, GeneDerive::None, false, "Cosmetic. Inherited and mutated, so it works as a lineage marker." },
		{ 1, "Outer G", GeneGroup::Colour, GeneFmt::Whole, GeneDerive::None, false, "Cosmetic. Inherited and mutated, so it works as a lineage marker." },
		{ 2, "Outer B", GeneGroup::Colour, GeneFmt::Whole, GeneDerive::None, false, "Cosmetic. Inherited and mutated, so it works as a lineage marker." },
		{ 3, "Inner R", GeneGroup::Colour, GeneFmt::Whole, GeneDerive::None, false, "Cosmetic. Inherited and mutated, so it works as a lineage marker." },
		{ 4, "Inner G", GeneGroup::Colour, GeneFmt::Whole, GeneDerive::None, false, "Cosmetic. Inherited and mutated, so it works as a lineage marker." },
		{ 5, "Inner B", GeneGroup::Colour, GeneFmt::Whole, GeneDerive::None, false, "Cosmetic. Inherited and mutated, so it works as a lineage marker." },
	} };

	template <size_t Extra>
	constexpr std::array<GeneInfo, 6 + Extra> with_shared(const std::array<GeneInfo, Extra>& extra)
	{
		std::array<GeneInfo, 6 + Extra> out{};
		for (size_t i = 0; i < 6; ++i)     out[i] = shared_colour_info[i];
		for (size_t i = 0; i < Extra; ++i) out[6 + i] = extra[i];
		return out;
	}

	template <size_t N>
	constexpr bool ids_match(const std::array<GeneInfo, N>& table)
	{
		for (size_t i = 0; i < N; ++i)
			if (table[i].id != i)
				return false;
		return true;
	}
}

// ---------------- Cell genes (must follow the CellGene enum order) ---------------- //
inline constexpr auto k_cell_gene_info = gene_info_detail::with_shared(std::array<GeneInfo, CellExtraGenes>{ {
	{ CellGenome::Amplitude, "Amplitude", GeneGroup::FrictionWave, GeneFmt::Plain, GeneDerive::None, true,
		"Swing of the grip wave: level = clamp(shift + amplitude * sin(phase), 0, 1). 0 = glide, 1 = grip." },
	{ CellGenome::Frequency, "Frequency", GeneGroup::FrictionWave, GeneFmt::Fine, GeneDerive::Period, true,
		"Wave speed in radians per frame. One cycle takes 2*pi / f frames." },
	{ CellGenome::Offset, "Offset", GeneGroup::FrictionWave, GeneFmt::Plain, GeneDerive::Degrees, true,
		"Starting phase of the grip wave in radians. Wraps at +-pi." },
	{ CellGenome::VerticalShift, "Vertical shift", GeneGroup::FrictionWave, GeneFmt::Plain, GeneDerive::None, true,
		"Centre of the grip wave." },

	{ CellGenome::BirthEnergyThresh, "Birth energy", GeneGroup::Reproduction, GeneFmt::Plain, GeneDerive::EnergyAbs, true,
		"Fraction of max energy this cell needs before it counts as ready to reproduce." },
	{ CellGenome::BirthIntegrityThresh, "Birth integrity", GeneGroup::Reproduction, GeneFmt::Plain, GeneDerive::IntegrityAbs, true,
		"Fraction of max integrity this cell needs before it counts as ready to reproduce." },
	{ CellGenome::BirthNutrientsThresh, "Birth nutrients", GeneGroup::Reproduction, GeneFmt::Plain, GeneDerive::NutrientsAbs, true,
		"Fraction of max nutrients this cell needs before it counts as ready to reproduce." },
	{ CellGenome::OffspringEnergyToGive, "Energy to child", GeneGroup::Reproduction, GeneFmt::Plain, GeneDerive::EffectiveGive, true,
		"Fraction of the parent's energy a child receives. The sim clamps it to 10-90%." },

	{ CellGenome::ConnectiveTendonSpringConst, "Tendon const", GeneGroup::NewbornTendon, GeneFmt::Plain, GeneDerive::None, true,
		"Spring constant of tendons this cell creates when it reproduces." },
	{ CellGenome::ConnectiveTendonDamping, "Tendon damping", GeneGroup::NewbornTendon, GeneFmt::Plain, GeneDerive::None, true,
		"Damping of tendons this cell creates when it reproduces." },

	{ CellGenome::AddCellChance, "Add cell", GeneGroup::StructuralMutation, GeneFmt::Plain, GeneDerive::Percent, true,
		"Chance, each time this cell is born, that it also grows an extra weak cell." },
	{ CellGenome::RemoveCellChance, "Remove cell", GeneGroup::StructuralMutation, GeneFmt::Plain, GeneDerive::Percent, true,
		"Chance, each time this cell is born, that it kills itself." },
	{ CellGenome::AddTendonChance, "Add tendon", GeneGroup::StructuralMutation, GeneFmt::Plain, GeneDerive::Percent, true,
		"Chance, each time this cell is born, that it links to a random cell of the organism." },
	{ CellGenome::RemoveTendonChance, "Remove tendon", GeneGroup::StructuralMutation, GeneFmt::Plain, GeneDerive::Percent, true,
		"Chance, each time this cell is born, that one of its tendons breaks." },

	{ CellGenome::NewbornSearchRadius, "Newborn search", GeneGroup::Social, GeneFmt::Tenth, GeneDerive::None, true,
		"Search radius a newborn uses when looking for cells to link to." },
	{ CellGenome::QuorumFraction, "Quorum", GeneGroup::Social, GeneFmt::Plain, GeneDerive::Percent, true,
		"Fraction of the organism that must be ready before it reproduces. The sim averages this over all its cells." },
	
	{ CellGenome::Mass, "Mass", GeneGroup::Body, GeneFmt::Tenth, GeneDerive::None, true,
	"Body mass. Sets the radius, so it also sets collision size and inertia." },

	{ CellGenome::InitialGrowthRate, "Growth rate", GeneGroup::Body, GeneFmt::Plain, GeneDerive::None, true,
		"How fast the cell grows toward its target mass." },
	{ CellGenome::GrowthDecay, "Growth decay", GeneGroup::Body, GeneFmt::Plain, GeneDerive::None, true,
		"How fast the decays the growth rate." }
	} });

// ---------------- Tendon genes (must follow the TendonGene enum order) ---------------- //
inline constexpr auto k_tendon_gene_info = gene_info_detail::with_shared(std::array<GeneInfo, TendonExtraGenes>{ {
	{ TendonGenome::Amplitude, "Amplitude", GeneGroup::ExtensionWave, GeneFmt::Plain, GeneDerive::None, true,
		"Swing of the rest-length wave: extension = clamp(shift + amplitude * sin(phase), 0, 1) of the tendon's extension span." },
	{ TendonGenome::Frequency, "Frequency", GeneGroup::ExtensionWave, GeneFmt::Fine, GeneDerive::Period, true,
		"Wave speed in radians per frame. One cycle takes 2*pi / f frames. Negative runs the wave backwards." },
	{ TendonGenome::Offset, "Offset", GeneGroup::ExtensionWave, GeneFmt::Plain, GeneDerive::Degrees, true,
		"Starting phase of the extension wave in radians. Wraps at +-pi." },
	{ TendonGenome::VerticalShift, "Vertical shift", GeneGroup::ExtensionWave, GeneFmt::Plain, GeneDerive::None, true,
		"Centre of the extension wave." },

	{ TendonGenome::NutrientTransferRate, "Transfer rate", GeneGroup::Nutrients, GeneFmt::Plain, GeneDerive::None, true,
		"Nutrients moved per tick between the two cells. Negative values are treated as 0 by the sim." },

	{ TendonGenome::SpringConst, "Spring const", GeneGroup::Mechanics, GeneFmt::Plain, GeneDerive::None, true,
		"Stiffness. Ramps up with the tendon's age until it is fully developed." },
	{ TendonGenome::Damping, "Damping", GeneGroup::Mechanics, GeneFmt::Plain, GeneDerive::None, true,
		"Damping along the tendon. The sim caps it by the reduced mass of the two cells." },

	{ TendonGenome::PhaseSyncRate, "Phase sync", GeneGroup::Sync, GeneFmt::Plain, GeneDerive::None, true,
		"How hard this tendon pulls its two cells (and its own wave) into step. 0 = no coupling." },
	} });

static_assert(k_cell_gene_info.size() == CellGenome::gene_count, "k_cell_gene_info size does not match the genome");
static_assert(k_tendon_gene_info.size() == TendonGenome::gene_count, "k_tendon_gene_info size does not match the genome");
static_assert(gene_info_detail::ids_match(k_cell_gene_info), "k_cell_gene_info is out of order with the CellGene enum");
static_assert(gene_info_detail::ids_match(k_tendon_gene_info), "k_tendon_gene_info is out of order with the TendonGene enum");