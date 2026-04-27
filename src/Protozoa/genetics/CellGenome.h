#pragma once
#include <cstdint>

#include "../../Utils/random.h"


struct Range
{
    float min, max;
    float clamp(float v) const { return std::clamp(v, min, max); }
};


struct GeneticConstraints
{
    inline static Range radius = { 15.f,        220.f };
    inline static float radius_step = 5.f;

    inline static Range amplitude = { -2.f,        2.f };
    inline static Range frequency = { -1.f / 30.f, 1.f / 30.f };
    inline static Range offset = { -3.14159f,   3.14159f };
    inline static Range vertical_shift = { -0.5f,       0.5f };
};


struct HardConstants
{
    inline static float add_cell_chance = 0.03f;
    inline static float remove_cell_chance = 0.03f;
    inline static float add_spring_chance = 0.03f;
    inline static float remove_spring_chance = 0.03f;

    inline static std::uint8_t transparency = 140;

    inline static float mutation_rate_rate = 0.1f;
    inline static float mutation_rate_range = 0.01f;
    inline static float colour_mutation_range = 0.001f;
};


struct CellGenome : HardConstants
{
    int generation = 0;

    float mutation_rate = 0.2f;
    float mutation_range = 0.2f;

    float radius = 90.f;

    uint8_t outer_r = Random::rand_byte();
    uint8_t outer_g = Random::rand_byte();
    uint8_t outer_b = Random::rand_byte();
    uint8_t inner_r = Random::rand_byte();
    uint8_t inner_g = Random::rand_byte();
    uint8_t inner_b = Random::rand_byte();

    float amplitude = 0.3f;
    float frequency = 1.f / 120.f;
    float offset = 0.f;
    float vertical_shift = 0.5f;

    CellGenome()
    {
     
    }

    void Randomize(const float deviation)
    {
        auto mutate = [&](float val, Range limits) -> float {
            return limits.clamp(val + Random::rand_range(limits.min, limits.max) * deviation);};

        mutation_rate = Random::rand_range(0.f, 0.3f);
        mutation_range = Random::rand_range(0.f, 0.3f);

        radius = mutate(radius, GeneticConstraints::radius);
        amplitude = mutate(amplitude, GeneticConstraints::amplitude);
        frequency = mutate(frequency, GeneticConstraints::frequency);
        offset = mutate(offset, GeneticConstraints::offset); 
        vertical_shift = mutate(vertical_shift, GeneticConstraints::vertical_shift);

        outer_r = Random::rand_byte(); outer_g = Random::rand_byte(); outer_b = Random::rand_byte();
        inner_r = Random::rand_byte(); inner_g = Random::rand_byte(); inner_b = Random::rand_byte();
    }

    void mutate(float mutation_rate_ = 0.f, float mutation_range_ = 0.f)
    {
        // if the mutation rate and range are undefined we use are own
		mutation_rate_ = mutation_rate_ > 0.f ? mutation_rate_ : mutation_rate;
		mutation_range_ = mutation_range_ > 0.f ? mutation_range_ : mutation_range;

        auto maybe_mutate = [&](float val, Range limits) -> float {
            if (Random::rand_range(0.f, 1.f) < mutation_rate_) return val;
            return limits.clamp(val + Random::rand_range(-mutation_range_, mutation_range_));
            };

        radius = maybe_mutate(radius, GeneticConstraints::radius);
        amplitude = maybe_mutate(amplitude, GeneticConstraints::amplitude);
        frequency = maybe_mutate(frequency, GeneticConstraints::frequency);
        offset = maybe_mutate(offset, GeneticConstraints::offset);
        vertical_shift = maybe_mutate(vertical_shift, GeneticConstraints::vertical_shift);

        auto chance = [](float rate) { return Random::rand01_float() < rate; };
        auto rand_sym = [](float range) { return Random::rand_range(-range, range); };
        mutation_rate += rand_sym(mutation_rate_range) * chance(mutation_rate_rate);
        mutation_range += rand_sym(mutation_rate_range) * chance(mutation_rate_rate);

        auto mutate_channel = [](uint8_t ch, float range) -> uint8_t {
            const int delta = static_cast<int>(Random::rand_range(-range * 255.f, range * 255.f));
            return static_cast<uint8_t>(std::clamp(static_cast<int>(ch) + delta, 0, 255));
            };
        outer_r = mutate_channel(outer_r, colour_mutation_range);
        outer_g = mutate_channel(outer_g, colour_mutation_range);
        outer_b = mutate_channel(outer_b, colour_mutation_range);
        inner_r = mutate_channel(inner_r, colour_mutation_range);
        inner_g = mutate_channel(inner_g, colour_mutation_range);
        inner_b = mutate_channel(inner_b, colour_mutation_range);
    }
};