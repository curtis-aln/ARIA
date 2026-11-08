#include "spring.h"

#include <algorithm>
#include <cmath>

void Spring::reset()
{
	cell_A_id = 0;
	cell_B_id = 0;

	work_done = 0.f;
	current_length = 0.f;
	rest_length = 0.f;
	stress = 0.f;
	spring_force = 0.f;
	damping_force = 0.f;

	movement_vector = { 0, 0 };
}

void Spring::update_physics(const CellPosVel& cell_data, const physics_toggles& toggles, const physics_stats& stats)
{
	// unpack the data for convenience
	const sf::Vector2f& pos_a = cell_data.cell_a_pos;
	const sf::Vector2f& vel_a = cell_data.cell_a_vel;
	const sf::Vector2f& pos_b = cell_data.cell_b_pos;
	const sf::Vector2f& vel_b = cell_data.cell_b_vel;
	
	const bool  disable_length_breakage = toggles.disable_length_breakage;
	const bool  disable_force_breakage = toggles.disable_force_breakage;

	
	const sf::Vector2f dir = pos_b - pos_a;
	const float length_squared = dir.x * dir.x + dir.y * dir.y;

	const float span = std::max(maximum_extension - stats.min_rest_length, 0.f);
	const float break_len = stats.min_rest_length + spring_break_length_factor * span;
	strain = (current_length - stats.min_rest_length) / (spring_break_length_factor * span);

	if (length_squared < 1e-6f) // avoid 1/0 -> NaN
	{
		movement_vector = { 0, 0 };
		stress = 0.f;    // don't leave last tick's values behind for the organics step to act on
		work_done = 0.f;
		return;
	}

	// finding the rest length of the spring
	rest_length = stats.min_rest_length + stats.extension_fraction * span;

	current_length = fast_sqrt(length_squared);
	const float length_diff = current_length - rest_length;

	// Calculating the spring force: Fs = K * (|B - A| - L)
	spring_force = stats.spring_constant * length_diff;

	// Calculating the damping force
	const float inv_length = 1.0f / current_length;
	const sf::Vector2f normalised_dir{ dir.x * inv_length, dir.y * inv_length };
	const sf::Vector2f vel_difference = (vel_b - vel_a);

	// Reduced mass, m_a*m_b / (m_a + m_b), is the single effective mass that makes two bodies joined by a spring
	// behave like one body of that mass, so the spring's stretch and compression respond to it.
	// It is always smaller than the lighter of the two masses, so the lighter cell dominates how fast the spring
	// oscillates and how easily damping overshoots.
	// That's why K / m_reduced and c / m_reduced are the right quantities to check for stability, not either cell's mass alone.
	const float damping = std::min(stats.damping, stats.reduced_mass);   // c / m_reduced <= 1 per tick
	damping_force = normalised_dir.dot(vel_difference) * damping;

	// Calculating total force (sum of the two forces)
	const float total_force = spring_force + damping_force;
	movement_vector = normalised_dir * total_force;

	// we can calculate the amount of energy this contraction / extension took
	work_done = std::abs(spring_force * length_diff);
	work_done *= spring_work_const;

	const float force_magnitude = std::abs(total_force);

	// Stress: 0 = relaxed, 1 = at breaking point
	stress = force_magnitude / spring_break_force;
}

