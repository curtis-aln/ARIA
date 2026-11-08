#pragma once

// a faster implementation of the round function
inline float fast_round(float x)
{
	return x >= 0.0f ? floorf(x + 0.5f) : ceilf(x - 0.5f);
}

inline float fast_sqrt(float x)
{
	float y = x;

	// Initial approximation
	uint32_t i;
	std::memcpy(&i, &y, sizeof(float));

	i = (i + 0x3f800000) >> 1;

	std::memcpy(&y, &i, sizeof(float));

	// Newton refinement
	return 0.5f * (y + x / y);
}

inline float fast_sin(float x)
{
	constexpr float PI = 3.14159265358979323846f;
	constexpr float TWO_PI = 6.28318530717958647692f;

	// Wrap to [-PI, PI]
	x = x - TWO_PI * floorf((x + PI) / TWO_PI);

	// Bhaskara I approximation
	float y = (16.0f * x * (PI - fabsf(x))) /
		(5.0f * PI * PI - 4.0f * fabsf(x) * (PI - fabsf(x)));

	return y;
}
