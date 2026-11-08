#include "spring.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "../cell/cell.h"
#include <entities/cell/cell_settings.h>
#include <Utils/random.h>

void Spring::reset()
{
	cell_A_id = 0;
	cell_B_id = 0;

	internal_clock_ = 0;
	work_done = 0.f;
	current_length = 0.f;
	rest_length = 0.f;
	stress = 0.f;
	broken = false;
	spring_force = 0.f;
	damping_force = 0.f;
	ratio = 0.f;

	genome.frequency = 0.f;
	genome.offset = 0.f;
	genome.vertical_shift = 1.f;
	genome.amplitude = 0.f;
	genome.spring_const = 0.f;
	genome.damping = 0.f;
	genome.nutrient_transfer_rate = 0.f;

	movement_vector = { 0, 0 };

	death_chance_ = 0.f;
}

void Spring::update_physics(
	const sf::Vector2f& pos_a, const sf::Vector2f& vel_a,
	const sf::Vector2f& pos_b, const sf::Vector2f& vel_b,
	bool disable_length_breakage, bool disable_force_breakage, float min_rest_length, const float reduced_mass)
{
	// min_rest_length is the distance between the two cells when they are just touching
	internal_clock_++;

	const sf::Vector2f dir = pos_b - pos_a;
	const float length_squared = dir.x * dir.x + dir.y * dir.y;

	const float span = std::max(maximum_extension - min_rest_length, 0.f);
	const float break_len = min_rest_length + spring_break_length_factor * span; 

	if (length_squared > break_len * break_len && !disable_length_breakage)
	{
		break_spring();
		movement_vector = { 0, 0 };
		return;
	}

	if (length_squared < 1e-6f) 
	{ 
		movement_vector = { 0, 0 }; 
		return; 
	} // avoid 1/0 -> NaN)

	// finding the rest length of the spring
	rest_length = min_rest_length + calculate_rest_length(internal_clock_) * span;

	current_length = fast_sqrt(length_squared);
	if (current_length < 1e-6f) { movement_vector = { 0, 0 }; return; } // avoid 1/0 -> NaN
	const float length_diff = current_length - rest_length;


	// Calculating the spring force: Fs = K * (|B - A| - L)
	spring_force = get_spring_constant() * length_diff;

	// Calculating the damping force
	const float inv_length = 1.0f / current_length;
	const sf::Vector2f normalised_dir{ dir.x * inv_length, dir.y * inv_length };
	const sf::Vector2f vel_difference = (vel_b - vel_a);

	// Reduced mass, m_a·m_b / (m_a + m_b), is the single effective mass that makes two bodies joined by a spring behave like one body of that mass, so the spring’s stretch and compression respond to it.
	//It is always smaller than the lighter of the two masses, so the lighter cell dominates how fast the spring oscillates and how easily damping overshoots.
	// That’s why K / m_reduced and c / m_reduced are the right quantities to check for stability, not either cell’s mass alone.
	const float damping = std::min(genome.damping, reduced_mass);   // c / m_reduced <= 1 per tick
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

	// Force-based break (complements your existing length-based break)
	if (force_magnitude > spring_break_force && !disable_force_breakage)
		break_spring();
}

void Spring::death_update(Cell& cell_a, Cell& cell_b)
{
	if (Random::rand01_float() < death_chance_)
	{
		break_spring();
		return;
	}

	if (!cell_a.is_alive() || !cell_b.is_alive())
	{
		break_spring();
		return;
	}
}

void Spring::update_organics(Cell& cell_a, Cell& cell_b, bool stress_damage, bool work_done_energy)
{
	death_update(cell_a, cell_b);

	if (stress > spring_damage_threshold && stress_damage)
	{
		update_integrity(cell_a, cell_b);
	}

	transfer_nutrients(cell_a, cell_b);
	sync_clocks(cell_a, cell_b);

	if (work_done_energy)
	{
		float energy_cost = -work_done * spring_work_energy_loss_k;
		cell_a.change_energy(energy_cost);
		cell_b.change_energy(energy_cost);
	}
}

void Spring::update_integrity(Cell& cell_a, Cell& cell_b)
{
	float excess = stress - spring_damage_threshold;
	float damage = -excess / 2.f * stress_damage_const;

	cell_a.change_integrity(damage);
	cell_b.change_integrity(damage);
	cell_a.cumulative_spring_damage_ += std::abs(damage);
	cell_b.cumulative_spring_damage_ += std::abs(damage);
}

float Spring::get_spring_constant()
{
	const float growth = std::clamp(static_cast<float>(internal_clock_) / fully_developed_age, 0.f, 1.f);
	return genome.spring_const * growth;
}
// Decides who sends to whom. Normally nutrients flow high -> low, but if exactly one cell
// has recently reproduced, nutrients are prioritised toward the one that hasn't.
Spring::NutrientFlow Spring::pick_nutrient_flow(Cell& cell_a, Cell& cell_b) const
{
	float& nutrients_a = cell_a.nutrients_;
	float& nutrients_b = cell_b.nutrients_;

	const bool b_higher = nutrients_b > nutrients_a;
	float* gradient_sender = b_higher ? &nutrients_b : &nutrients_a;
	float* gradient_receiver = b_higher ? &nutrients_a : &nutrients_b;

	// "recently reproduced" == still inside its post-reproduction cooldown window
	const bool a_reproduced = !cell_a.check_repro_cooldown();
	const bool b_reproduced = !cell_b.check_repro_cooldown();

	if (a_reproduced == b_reproduced)
		return { gradient_sender, gradient_receiver, false };

	float* desired_sender = a_reproduced ? &nutrients_a : &nutrients_b;
	float* desired_receiver = a_reproduced ? &nutrients_b : &nutrients_a;

	// priority = we're overriding the gradient, so the diff > 0 check must be skipped
	return { desired_sender, desired_receiver, gradient_sender != desired_sender };
}

// How much can actually move this tick. Returns 0 if nothing should be sent.
float Spring::calc_nutrient_send_amount(const NutrientFlow& flow, const float rate) const
{
	const float diff = *flow.sender - *flow.receiver;
	if (diff <= 0.0f && !flow.priority)
		return 0.0f;

	const float space = CellSettings::max_nutrients - *flow.receiver;
	if (space <= 0.0f)
		return 0.0f;

	// the sender must send enough that, after loss, the receiver doesn't exceed max_nutrients
	const float max_by_space = space / (1.0f - nutrients_transfer_loss);

	float amount = std::min({ rate, *flow.sender, max_by_space });
	if (!flow.priority)
		amount = std::min(amount, diff);   // gradient transfers never overshoot equilibrium
	return amount;
}


void Spring::transfer_nutrients(Cell& cell_a, Cell& cell_b)
{
	const float rate = std::max(genome.nutrient_transfer_rate, 0.0f);
	if (rate <= 0.0f)
		return;

	const NutrientFlow flow = pick_nutrient_flow(cell_a, cell_b);
	const float send_amount = calc_nutrient_send_amount(flow, rate);

	*flow.sender -= send_amount;
	*flow.receiver += send_amount * (1.0f - nutrients_transfer_loss);
}

void Spring::sync_clocks(Cell & cell_a, Cell & cell_b)
{
	// Slows the leading clock and speeds up the lagging one until they meet in the middle.
	static constexpr float sync_rate = 0.01f; // fraction of the gap each clock closes per tick

	uint32_t& clock_a = cell_a.internal_clock_;
	uint32_t& clock_b = cell_b.internal_clock_;

	if (clock_a == clock_b)
		return;

	// Unsigned subtraction wraps, so the gap stays correct even if a clock has overflowed.
	// Gaps of 2^31 or more are read as "the other one is behind".
	const uint32_t d = clock_b - clock_a;
	const bool a_behind = d < 0x80000000u;
	const uint32_t gap = a_behind ? d : (0u - d);

	uint32_t& behind = a_behind ? clock_a : clock_b;
	uint32_t& ahead = a_behind ? clock_b : clock_a;

	// Clocks are integers, so gap * 1% rounds to 0 for small gaps. Move at least 1 tick,
	// but never more than half the gap, otherwise the two clocks would swap places.
	const uint32_t step = std::min(std::max(1u, static_cast<uint32_t>(gap * sync_rate)), gap / 2);

	if (step == 0)   // gap == 1: halves can't split it, so the lagging clock takes the last tick
	{
		++behind;
		return;
	}

	behind += step;
	ahead -= step;
}

float Spring::calculate_rest_length(const int internal_clock)
{
	// sin oscillates around vertical_shift with ±amplitude swing
	const float sin_value = fast_sin(genome.frequency * internal_clock + genome.offset); // [-1, 1]
	ratio = genome.vertical_shift + genome.amplitude * sin_value;     // [vs-a, vs+a]
	return std::clamp(ratio, 0.f, 1.f);
}