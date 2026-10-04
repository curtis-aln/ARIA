#pragma once
#include "../../Utils/random.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

struct Range
{
	float min, max;
	constexpr float clamp(float v) const { return std::clamp(v, min, max); }
	constexpr bool contains(const Range& inner) const
	{
		return inner.min <= inner.max && inner.min >= min && inner.max <= max;
	}
};

// ---------------------------------------------------------------------------
// Shared base: every genome type (Cell, Spring, future types) inherits this.
// ---------------------------------------------------------------------------
struct BaseConstants
{
	inline static constexpr float colour_mutation_range = 0.01f;
	inline static constexpr float mutation_rate_range = 0.006f;

	inline static constexpr Range init_mutation_range_spread = { 0.005f, 0.045f };

	inline static constexpr Range gaussian_const_limits = { 0.05f, 0.5f };   // hard evolutionary bounds
	inline static constexpr Range init_gaussian_const_spread = { 0.01f, 0.065f };  // spawn range
};

struct GenomeBase : protected BaseConstants
{
	float guassian_const, mutation_range;
	uint32_t generation = 0;

	uint8_t outer_r, outer_g, outer_b, inner_r, inner_g, inner_b;

	GenomeBase()
	{
		randomize_base();
	}

	void randomize_base()
	{
		rand_in_range(guassian_const, init_gaussian_const_spread);
		rand_in_range(mutation_range, init_mutation_range_spread);

		outer_r = Random::rand_byte(); outer_g = Random::rand_byte(); outer_b = Random::rand_byte();
		inner_r = Random::rand_byte(); inner_g = Random::rand_byte(); inner_b = Random::rand_byte();
	}

protected:
	float maybe_mutate(float val, Range limits, float rate, float range) const
	{
		if (Random::rand01_float() >= rate)
			return val;
		return limits.clamp(val + Random::rand_range(-range, range));
	}

	// ES-style Gaussian mutation. Unlike maybe_mutate, this never "skips" —
	// it always perturbs val, drawing the offset from a normal distribution
	// with std-dev = range * gaussian_const, instead of a uniform draw gated by rate.
	// Most nudges land close to 0; large jumps are rare but not impossible.
	float maybe_mutate_gaussian(float val, Range limits, float range, float gaussian_const) const
	{
		const float sigma = range * gaussian_const;
		if (sigma <= 0.f)
			return val;

		return limits.clamp(val + sample_gaussian() * sigma);
	}

	void mutate_meta()
	{
		auto nudge = [](float v, float range) {
			return v + Random::rand_range(-range, range); };

		guassian_const = nudge(guassian_const, mutation_rate_range);
		mutation_range = nudge(mutation_range, mutation_rate_range);

		mutate_colour(outer_r, outer_g, outer_b);
		mutate_colour(inner_r, inner_g, inner_b);
	}

	void rand_in_range(float& val, const Range& range)
	{
		val = Random::rand_range(range.min, range.max);
	}


private:
	// Box-Muller transform: converts two uniform [0,1) samples into one
	// standard-normal sample (mean 0, std-dev 1).
	static float sample_gaussian()
	{
		const float u1 = std::max(Random::rand01_float(), 1e-7f); // avoid log(0)
		const float u2 = Random::rand01_float();
		constexpr float two_pi = 6.28318530718f;
		return std::sqrt(-2.f * std::log(u1)) * std::cos(two_pi * u2);
	}

	void mutate_colour(uint8_t& r, uint8_t& g, uint8_t& b) const
	{
		auto shift = [](uint8_t ch, float range) -> uint8_t {
			const int delta = static_cast<int>(Random::rand_range(-range * 255.f, range * 255.f));
			return static_cast<uint8_t>(std::clamp(static_cast<int>(ch) + delta, 0, 255));
			};
		r = shift(r, colour_mutation_range);
		g = shift(g, colour_mutation_range);
		b = shift(b, colour_mutation_range);
	}
};

// ---------------------------------------------------------------------------
// Spring genome
// ---------------------------------------------------------------------------
struct SpringGeneticConstraints
{
	inline static constexpr Range amplitude = { 0.f,          1.f };
	inline static constexpr Range frequency = { -1.f / 120.f, 1.f };
	inline static constexpr Range offset = { -3.14159f,    3.14159f };
	inline static constexpr Range vertical_shift = { -1.f,        1.f };
	inline static constexpr Range spring_const = { 0.f,          1.f };
	inline static constexpr Range damping = { 0.f,          1.f };
	inline static constexpr Range nutrient_transfer_rate = { -1.f,          1.f };
};

struct SpringInitialSpawnRanges
{
	inline static constexpr Range amplitude = { 0.1f,         0.7f };
	inline static constexpr Range frequency = { 1.f / 60.f,  1.f / 2.f };
	inline static constexpr Range offset = SpringGeneticConstraints::offset;
	inline static constexpr Range vertical_shift = { 0.7f,          0.7f };

	inline static constexpr Range spring_const = { 0.01f, 0.3f };
	inline static constexpr Range damping = { 0.01f, 0.1f };

	inline static constexpr Range nutrient_transfer_rate = { -0.1f, 0.5f };
};

struct SpringGenome : GenomeBase
{
	float amplitude, frequency, offset, vertical_shift;
	float nutrient_transfer_rate;
	float spring_const, damping;

	SpringGenome() { randomize(); };

	void randomize()
	{
		randomize_base();

		using Limit = SpringInitialSpawnRanges;

		rand_in_range(amplitude, Limit::amplitude);
		rand_in_range(frequency, Limit::frequency);
		rand_in_range(offset, Limit::offset);
		rand_in_range(vertical_shift, Limit::vertical_shift);

		rand_in_range(spring_const, Limit::spring_const);
		rand_in_range(damping, Limit::damping);

		rand_in_range(nutrient_transfer_rate, Limit::nutrient_transfer_rate);
	}

	void mutate(float range = 0.f)
	{
		range = range > 0.f ? range : mutation_range;

		const auto& C = SpringGeneticConstraints{};

		amplitude = maybe_mutate_gaussian(amplitude, C.amplitude, range, guassian_const);
		frequency = maybe_mutate_gaussian(frequency, C.frequency, range, guassian_const);
		offset = maybe_mutate_gaussian(offset, C.offset, range, guassian_const);
		vertical_shift = maybe_mutate_gaussian(vertical_shift, C.vertical_shift, range, guassian_const);

		nutrient_transfer_rate = maybe_mutate_gaussian(nutrient_transfer_rate, C.nutrient_transfer_rate, range, guassian_const);

		spring_const = maybe_mutate_gaussian(spring_const, C.spring_const, range, guassian_const);
		damping = maybe_mutate_gaussian(damping, C.damping, range, guassian_const);

		mutate_meta();
	}

	void copy_genetics(const SpringGenome& parent)
	{
		amplitude = parent.amplitude;
		frequency = parent.frequency;
		offset = parent.offset;
		vertical_shift = parent.vertical_shift;

		spring_const = parent.spring_const;
		damping = parent.damping;

		nutrient_transfer_rate = parent.nutrient_transfer_rate;

		guassian_const = parent.guassian_const;
		mutation_range = parent.mutation_range;

		outer_r = parent.outer_r; outer_g = parent.outer_g; outer_b = parent.outer_b;
		inner_r = parent.inner_r; inner_g = parent.inner_g; inner_b = parent.inner_b;
	}

	void sexually_reproduce(const SpringGenome& a, const SpringGenome& b)
	{
		auto mid = [](float x, float y) { return (x + y) * 0.5f; };
		auto midc = [](uint8_t x, uint8_t y) { return static_cast<uint8_t>((int(x) + int(y)) / 2); };

		amplitude = mid(a.amplitude, b.amplitude);
		frequency = mid(a.frequency, b.frequency);
		offset = mid(a.offset, b.offset);
		vertical_shift = mid(a.vertical_shift, b.vertical_shift);
		spring_const = mid(a.spring_const, b.spring_const);
		damping = mid(a.damping, b.damping);
		nutrient_transfer_rate = mid(a.nutrient_transfer_rate, b.nutrient_transfer_rate);

		guassian_const = mid(a.guassian_const, b.guassian_const);
		mutation_range = mid(a.mutation_range, b.mutation_range);
		generation = std::max(a.generation, b.generation) + 1;

		outer_r = midc(a.outer_r, b.outer_r); outer_g = midc(a.outer_g, b.outer_g); outer_b = midc(a.outer_b, b.outer_b);
		inner_r = midc(a.inner_r, b.inner_r); inner_g = midc(a.inner_g, b.inner_g); inner_b = midc(a.inner_b, b.inner_b);
	}
};

// ---------------------------------------------------------------------------
// Cell genome
// ---------------------------------------------------------------------------
struct CellGeneticConstraints
{
	inline static constexpr Range radius = { 25.f,         120.f };
	inline static constexpr Range amplitude = { -2.f,         2.f };
	inline static constexpr Range frequency = { 1.f / 60.f,  1.f / 2.f };
	inline static constexpr Range offset = { -3.14159f,    3.14159f };
	inline static constexpr Range vertical_shift = { -1.f,        1.f };

	inline static constexpr Range newborn_search = { 0.f, radius.max * 4.f };
};

struct CellInitialSpawnRanges
{
	inline static constexpr Range radius = { 25.f,          85.f };

	inline static constexpr Range amplitude = { 0.01f,          0.08f };
	inline static constexpr Range frequency = { 1.f / 60.f,  1.f / 2.f };
	inline static constexpr Range offset = CellGeneticConstraints::offset;
	inline static constexpr Range vertical_shift = { 0.965f,           0.99f };
};

struct HardConstants
{
	inline static constexpr float     add_cell_chance = 0.02f;
	inline static constexpr uint8_t   outer_transparency = 150;
	inline static constexpr uint8_t   inner_transparency = 125;

	inline static constexpr float radius_mutation_multiplier = 5.f;
	inline static constexpr float newborn_search_radius_multiplier = 5.f;
	inline static constexpr float friction_multiplier = 0.95f;
};

struct CellGenome : GenomeBase, HardConstants
{
	float radius, amplitude, frequency, offset, vertical_shift;

	// reproductive genes
	float birth_energy_thresh = 0.90f;
	float birth_integrity_thresh = 0.25f;
	float birth_nutrients_thresh = 0.1f;

	float offspring_energy_to_give = 0.25f;
	float connective_spring_spring_const = 0.1f;
	float connective_spring_damping = 0.1f;

	float newborn_search_radius = CellGeneticConstraints::newborn_search.max;

	CellGenome() { randomize(); };

	void randomize()
	{
		randomize_base();

		auto rand_in_range = [](float& val, const Range& range) {
			val = Random::rand_range(range.min, range.max);
			};

		using Limit = CellInitialSpawnRanges;
		using C_Const = CellGeneticConstraints;
		using S_Const = SpringGeneticConstraints;
		rand_in_range(amplitude, Limit::amplitude);
		rand_in_range(frequency, Limit::frequency);
		rand_in_range(offset, Limit::offset);
		rand_in_range(vertical_shift, Limit::vertical_shift);

		rand_in_range(radius, Limit::radius);

		rand_in_range(birth_energy_thresh, { 0.8f, .85f });
		rand_in_range(birth_integrity_thresh, { 0.f, 0.1f });
		rand_in_range(birth_nutrients_thresh, { 0.f, 0.1f });

		rand_in_range(offspring_energy_to_give, { 0.f, 1.f });
		rand_in_range(connective_spring_spring_const, S_Const::spring_const);
		rand_in_range(connective_spring_damping, S_Const::damping);

		rand_in_range(newborn_search_radius, { C_Const::newborn_search.max * 0.9f, C_Const::newborn_search.max });
	}

	void mutate(float range = 0.f)
	{
		range = range > 0.f ? range : mutation_range;

		const auto& C = CellGeneticConstraints{};

		radius = maybe_mutate_gaussian(radius, C.radius, range * radius_mutation_multiplier, guassian_const);
		amplitude = maybe_mutate_gaussian(amplitude, C.amplitude, range * friction_multiplier, guassian_const);
		frequency = maybe_mutate_gaussian(frequency, C.frequency, range * friction_multiplier, guassian_const);
		offset = maybe_mutate_gaussian(offset, C.offset, range * friction_multiplier, guassian_const);
		vertical_shift = maybe_mutate_gaussian(vertical_shift, C.vertical_shift, range * friction_multiplier, guassian_const);

		birth_energy_thresh = maybe_mutate_gaussian(birth_energy_thresh, { 0.f, 1.f }, range, guassian_const);
		birth_integrity_thresh = maybe_mutate_gaussian(birth_integrity_thresh, { 0.f, 1.f }, range, guassian_const);
		birth_nutrients_thresh = maybe_mutate_gaussian(birth_nutrients_thresh, { 0.f, 1.f }, range, guassian_const);

		offspring_energy_to_give = maybe_mutate_gaussian(offspring_energy_to_give, { 0.f, 1.f }, range, guassian_const);

		connective_spring_damping = maybe_mutate_gaussian(connective_spring_damping, SpringGeneticConstraints::damping, range, guassian_const);
		connective_spring_spring_const = maybe_mutate_gaussian(connective_spring_spring_const, SpringGeneticConstraints::spring_const, range, guassian_const);

		const float multip = newborn_search_radius_multiplier;
		const float r_max = CellGeneticConstraints::radius.max;
		newborn_search_radius = maybe_mutate_gaussian(newborn_search_radius, CellGeneticConstraints::newborn_search, range * multip, guassian_const);

		mutate_meta();
	}

	void copy_genetics(const CellGenome& parent)
	{
		amplitude = parent.amplitude;
		frequency = parent.frequency;
		offset = parent.offset;
		vertical_shift = parent.vertical_shift;
		radius = parent.radius;

		outer_r = parent.outer_r; outer_g = parent.outer_g; outer_b = parent.outer_b;
		inner_r = parent.inner_r; inner_g = parent.inner_g; inner_b = parent.inner_b;

		birth_energy_thresh = parent.birth_energy_thresh;
		birth_integrity_thresh = parent.birth_integrity_thresh;
		birth_nutrients_thresh = parent.birth_nutrients_thresh;

		offspring_energy_to_give = parent.offspring_energy_to_give;
		connective_spring_spring_const = parent.connective_spring_spring_const;
		connective_spring_damping = parent.connective_spring_damping;

		newborn_search_radius = parent.newborn_search_radius;

		guassian_const = parent.guassian_const;
		mutation_range = parent.mutation_range;
	}

};

// Compile-time guard: every spawn range must sit inside its evolutionary constraint,
// otherwise the first mutation clamps the child to the constraint (see the vertical_shift bug).
#define GENOME_RANGE_CHECK(Constraint, Spawn, Field) \
	static_assert(Constraint::Field.contains(Spawn::Field), \
		#Spawn "::" #Field " is outside " #Constraint "::" #Field)

// Cell
GENOME_RANGE_CHECK(CellGeneticConstraints, CellInitialSpawnRanges, radius);
GENOME_RANGE_CHECK(CellGeneticConstraints, CellInitialSpawnRanges, amplitude);
GENOME_RANGE_CHECK(CellGeneticConstraints, CellInitialSpawnRanges, frequency);
GENOME_RANGE_CHECK(CellGeneticConstraints, CellInitialSpawnRanges, offset);
GENOME_RANGE_CHECK(CellGeneticConstraints, CellInitialSpawnRanges, vertical_shift);

// Spring
GENOME_RANGE_CHECK(SpringGeneticConstraints, SpringInitialSpawnRanges, amplitude);
GENOME_RANGE_CHECK(SpringGeneticConstraints, SpringInitialSpawnRanges, frequency);
GENOME_RANGE_CHECK(SpringGeneticConstraints, SpringInitialSpawnRanges, offset);
GENOME_RANGE_CHECK(SpringGeneticConstraints, SpringInitialSpawnRanges, vertical_shift);
GENOME_RANGE_CHECK(SpringGeneticConstraints, SpringInitialSpawnRanges, spring_const);
GENOME_RANGE_CHECK(SpringGeneticConstraints, SpringInitialSpawnRanges, damping);
GENOME_RANGE_CHECK(SpringGeneticConstraints, SpringInitialSpawnRanges, nutrient_transfer_rate);

#undef GENOME_RANGE_CHECK