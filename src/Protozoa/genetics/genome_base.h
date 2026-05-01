#pragma once
#include "../../Utils/random.h"
#include "range.h"

// Behaviour shared by every genome type
struct GenomeBase
{
    float mutation_rate = 0.2f;
    float mutation_range = 0.2f;

    static constexpr float mutation_rate_rate = 0.1f;
    static constexpr float mutation_rate_range = 0.01f;

    int generation = 0;

protected:
    // Returns val unmutated OR val + noise, clamped to limits.
    float maybe_mutate(float val, Range limits, float rate, float range) const
    {
        if (Random::rand01_float() >= rate)   // did not roll a mutation
            return val;
        return limits.clamp(val + Random::rand_range(-range, range));
    }

    // Lets mutation_rate and mutation_range drift slowly over generations.
    void mutate_meta()
    {
        auto nudge = [](float v, float range) {
            return v + Random::rand_range(-range, range);
            };

        if (Random::rand01_float() < mutation_rate_rate)
            mutation_rate = std::clamp(nudge(mutation_rate, mutation_rate_range), 0.f, 1.f);
        if (Random::rand01_float() < mutation_rate_rate)
            mutation_range = std::clamp(nudge(mutation_range, mutation_rate_range), 0.f, 1.f);
    }
};
