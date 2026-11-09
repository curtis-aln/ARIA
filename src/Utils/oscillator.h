#pragma once

#include <cmath>

namespace osc
{
	inline constexpr float two_pi = 6.28318530718f;

	// wraps any angle into [0, 2*pi)
	inline float wrap_two_pi(float p)
	{
		return p - two_pi * std::floor(p / two_pi);
	}

	// shortest signed arc, in [-pi, pi]. 359 degrees ahead is 1 degree behind.
	inline float wrap_pi(float d)
	{
		return d - two_pi * std::round(d / two_pi);
	}

	// Pulls two phases toward each other along the shortest arc.
	// `rate` is the fraction of the gap closed per tick (0 = no coupling, 1 = snap together).
	inline void couple(float& a, float& b, const float rate)
	{
		const float pull = rate * 0.5f * wrap_pi(b - a);
		a += pull;
		b -= pull;
	}
}