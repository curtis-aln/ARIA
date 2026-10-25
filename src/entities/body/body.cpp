#include "body.h"
#include <cmath>

namespace
{
	// The two anchor points of the curve: mass_lo maps to radius_lo, mass_hi to radius_hi.
	// Keep the radii in step with CellGeneticConstraints::radius.
	constexpr float mass_lo = 50.f;
	constexpr float mass_hi = 150.f;
	constexpr float radius_lo = 55.f;
	constexpr float radius_hi = 135.f;
}

// Order matters: size_scale reads size_exponent, and both are initialised in this order.
const float Body::size_exponent = std::log(radius_hi / radius_lo) / std::log(mass_hi / mass_lo); // ~0.817
const float Body::size_scale = radius_lo / std::pow(mass_lo, size_exponent);

void Body::set_radius()
{
	radius_ = size_scale * std::pow(mass_, size_exponent);
}

void Body::set_mass()
{
	mass_ = std::pow(radius_ / size_scale, 1.f / size_exponent);
}

float Body::mass_to_radius(const float mass)
{
	return Body::size_scale * std::pow(mass, Body::size_exponent);
}