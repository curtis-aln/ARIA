#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <array>
#include <cassert>
#include "../../Utils/random.h"


inline float sample_gaussian()
{
	const float u1 = std::max(Random::rand01_float(), 1e-7f); // avoid log(0)
	const float u2 = Random::rand01_float();
	constexpr float two_pi = 6.28318530718f;
	return std::sqrt(-2.f * std::log(u1)) * std::cos(two_pi * u2);
}


inline float wrap01(float x)
{
	x -= std::floor(x);
	return (x >= 1.f) ? 0.f : x; // guards float rounding on tiny negatives
}

// Clamping makes the boundaries sticky. Any mutation that overshoots 0 or 1 lands exactly on the edge, so genes pile up at 0 and 1 over time. Reflecting at the boundary avoids this
inline float reflect01(float x)
{
	x = std::fabs(x);
	x = std::fmod(x, 2.f);
	return x > 1.f ? 2.f - x : x;
}

struct Range
{
	float min, max;
	float rand() const { return Random::rand_range(min, max); }

	constexpr bool contains(const Range& inner) const
	{
		return inner.min <= inner.max && inner.min >= min && inner.max <= max;
	}
};