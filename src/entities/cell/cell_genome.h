#pragma once
#include <cstdint>
#include "../../Utils/random.h"
#include "../genome_base.h"

struct CellGeneticConstraints
{
    inline static Range radius = { 15.f,         220.f };
    inline static float radius_step = 5.f;
    inline static Range amplitude = { -2.f,         2.f };
    inline static Range frequency = { -1.f / 30.f,  1.f / 30.f };
    inline static Range offset = { -3.14159f,    3.14159f };
    inline static Range vertical_shift = { -0.5f,        0.5f };
};

struct CellInitialSpawnRanges
{
    inline static Range radius = { 30.f,          100.f };
    inline static Range amplitude = { 0.5f,          1.f };
    inline static Range frequency = { 1.f / 60.f,   1.f / 10.f };
    inline static Range offset = CellGeneticConstraints::offset;
    inline static Range vertical_shift = { 0.f,           0.2f };
};

struct HardConstants
{
    inline static float     add_cell_chance = 0.03f;
    inline static float     remove_cell_chance = 0.03f;
    inline static float     add_spring_chance = 0.03f;
    inline static float     remove_spring_chance = 0.03f;
    inline static uint8_t   outer_transparency = 200;
    inline static uint8_t   inner_transparency = 100;
    inline static float     colour_mutation_range = 0.001f;
};

struct CellGenome : GenomeBase, HardConstants
{
    float radius = Random::rand_range(CellInitialSpawnRanges::radius.min, CellInitialSpawnRanges::radius.max);
    float amplitude = Random::rand_range(CellInitialSpawnRanges::amplitude.min, CellInitialSpawnRanges::amplitude.max);
    float frequency = Random::rand_range(CellInitialSpawnRanges::frequency.min, CellInitialSpawnRanges::frequency.max);
    float offset = Random::rand_range(CellInitialSpawnRanges::offset.min, CellInitialSpawnRanges::offset.max);
    float vertical_shift = Random::rand_range(CellInitialSpawnRanges::vertical_shift.min, CellInitialSpawnRanges::vertical_shift.max);

    uint8_t outer_r = Random::rand_byte(), outer_g = Random::rand_byte(), outer_b = Random::rand_byte();
    uint8_t inner_r = Random::rand_byte(), inner_g = Random::rand_byte(), inner_b = Random::rand_byte();

    CellGenome() = default;

    void randomize()
    {
        mutation_rate = Random::rand_range(0.f, 0.3f);
        mutation_range = Random::rand_range(0.f, 0.3f);

        amplitude = Random::rand_range(CellInitialSpawnRanges::amplitude.min, CellInitialSpawnRanges::amplitude.max);
        frequency = Random::rand_range(CellInitialSpawnRanges::frequency.min, CellInitialSpawnRanges::frequency.max);
        offset = Random::rand_range(CellInitialSpawnRanges::offset.min, CellInitialSpawnRanges::offset.max);
        vertical_shift = Random::rand_range(CellInitialSpawnRanges::vertical_shift.min, CellInitialSpawnRanges::vertical_shift.max);

        outer_r = Random::rand_byte(); outer_g = Random::rand_byte(); outer_b = Random::rand_byte();
        inner_r = Random::rand_byte(); inner_g = Random::rand_byte(); inner_b = Random::rand_byte();
    }

    void mutate(float rate = 0.f, float range = 0.f)
    {
        rate = rate > 0.f ? rate : mutation_rate;
        range = range > 0.f ? range : mutation_range;

        const auto& C = CellGeneticConstraints{};
        radius = maybe_mutate(radius, C.radius, rate, range);
        amplitude = maybe_mutate(amplitude, C.amplitude, rate, range);
        frequency = maybe_mutate(frequency, C.frequency, rate, range);
        offset = maybe_mutate(offset, C.offset, rate, range);
        vertical_shift = maybe_mutate(vertical_shift, C.vertical_shift, rate, range);

        mutate_meta();
        mutate_colour(outer_r, outer_g, outer_b);
        mutate_colour(inner_r, inner_g, inner_b);
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

        mutation_rate = parent.mutation_rate;
        mutation_range = parent.mutation_range;

    }

private:
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
