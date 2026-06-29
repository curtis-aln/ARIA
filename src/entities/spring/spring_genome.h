#pragma once
#include "../range.h"
#include "../genome_base.h"

struct SpringGeneticConstraints
{
    inline static Range amplitude = { 0.f,          1.f };  // ratio [0,1]
    inline static Range frequency = { -1.f / 40.f, 1.f / 40.f };
    inline static Range offset = { -3.14159f,    3.14159f };
    inline static Range vertical_shift = { -0.5f,        0.5f };
    inline static Range spring_const = { 0.f,          1.f };
    inline static Range damping = { 0.f,          1.f };
};

struct SpringInitialSpawnRanges
{
    inline static Range amplitude = { 0.3f,         0.5f };
    inline static Range frequency = { 1.f / 30.f,  1.f / 2.f };
    inline static Range offset = SpringGeneticConstraints::offset;
    inline static Range vertical_shift = { 0.7f,          0.7f };
    inline static Range spring_const = { 0.1f, 0.1f };
    inline static Range damping = { 0.4f,0.8f };
};

struct SpringGenome : GenomeBase
{
    float amplitude = Random::rand_range(SpringInitialSpawnRanges::amplitude.min, SpringInitialSpawnRanges::amplitude.max);
    float frequency = Random::rand_range(SpringInitialSpawnRanges::frequency.min, SpringInitialSpawnRanges::frequency.max);
    float offset = Random::rand_range(SpringInitialSpawnRanges::offset.min, SpringInitialSpawnRanges::offset.max);
    float vertical_shift = Random::rand_range(SpringInitialSpawnRanges::vertical_shift.min, SpringInitialSpawnRanges::vertical_shift.max);

    float spring_const = 0.05f;
    float damping = 0.6f;

    SpringGenome() = default;

    void randomize()
    {
        mutation_rate = Random::rand_range(0.f, 0.3f);
        mutation_range = Random::rand_range(0.f, 0.3f);

        amplitude = Random::rand_range(SpringInitialSpawnRanges::amplitude.min, SpringInitialSpawnRanges::amplitude.max);
        frequency = Random::rand_range(SpringInitialSpawnRanges::frequency.min, SpringInitialSpawnRanges::frequency.max);
        offset = Random::rand_range(SpringInitialSpawnRanges::offset.min, SpringInitialSpawnRanges::offset.max);
        vertical_shift = Random::rand_range(SpringInitialSpawnRanges::vertical_shift.min, SpringInitialSpawnRanges::vertical_shift.max);
    }

    void mutate(float rate = 0.f, float range = 0.f)
    {
        rate = rate > 0.f ? rate : mutation_rate;
        range = range > 0.f ? range : mutation_range;

        const auto& C = SpringGeneticConstraints{};

        amplitude = maybe_mutate(amplitude, C.amplitude, rate, range);
        frequency = maybe_mutate(frequency, C.frequency, rate, range);
        offset = maybe_mutate(offset, C.offset, rate, range);
        vertical_shift = maybe_mutate(vertical_shift, C.vertical_shift, rate, range);

        spring_const = maybe_mutate(spring_const, C.spring_const, rate, range);
        damping = maybe_mutate(damping, C.damping, rate, range);

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

        mutation_rate = parent.mutation_rate;
        mutation_range = parent.mutation_range;
    }

   
};