#include "tendon.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "../cell/cell.h"
#include <entities/cell/cell_settings.h>
#include <Utils/random.h>

#include "../../Utils/oscillator.h"


void Tendon::reset()
{
	Spring::reset();
	death_chance_ = 0.f;
	broken = false;

	extension_ratio = 0.f;
	phase_ = 0.f;

	internal_clock_ = 0;

	genome.frequency = 0.f;
	genome.offset = 0.f;
	genome.vertical_shift = 1.f;
	genome.amplitude = 0.f;
	genome.spring_const = 0.f;
	genome.damping = 0.f;
	genome.nutrient_transfer_rate = 0.f;
}

void Tendon::death_update(Cell& cell_a, Cell& cell_b)
{

	if (!cell_a.is_alive() || !cell_b.is_alive() || (death_chance_ > 0.f && Random::rand01_float() < death_chance_))
		break_tendon();
}

void Tendon::sync_oscillators(Cell& cell_a, Cell& cell_b)
{
	phase_ = osc::wrap_two_pi(phase_ + genome.frequency);

	const float rate = phase_sync_rate;
	if (rate <= 0.f)
		return;

	osc::couple(cell_a.phase_, cell_b.phase_, rate);
	osc::couple(phase_, cell_a.phase_, rate);
	osc::couple(phase_, cell_b.phase_, rate);
}

void Tendon::update_organics(Cell& cell_a, Cell& cell_b, bool stress_damage, bool work_done_energy)
{
	death_update(cell_a, cell_b);

	// min_rest_length is the distance between the two cells when they are just touching
	internal_clock_++;
	target_extension_fraction();

	// a spring that just snapped shouldn't still move nutrients or cost energy this tick
	if (is_tendon_broken())
		return;

	if (stress > spring_damage_threshold && stress_damage)
		update_integrity(cell_a, cell_b);

	transfer_nutrients(cell_a, cell_b);
	sync_oscillators(cell_a, cell_b);

	if (work_done_energy)
	{
		const float energy_cost = -work_done;
		cell_a.change_energy(energy_cost);
		cell_b.change_energy(energy_cost);
	}

	// Force-based break (complements the length-based break above)
	if (stress > 1.0 || strain > 1.0)
		break_tendon();
}

void Tendon::update_integrity(Cell& cell_a, Cell& cell_b)
{
	const float excess = stress - spring_damage_threshold;
	const float damage = -excess / 2.f * stress_damage_const;

	cell_a.change_integrity(damage);
	cell_b.change_integrity(damage);
	cell_a.cumulative_tendon_damage_ += std::abs(damage);
	cell_b.cumulative_tendon_damage_ += std::abs(damage);
}

// Decides who sends to whom. Normally nutrients flow high -> low, but if exactly one cell
// has recently reproduced, nutrients are prioritised toward the one that hasn't.
Tendon::NutrientFlow Tendon::pick_nutrient_flow(Cell& cell_a, Cell& cell_b) const
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
float Tendon::calc_nutrient_send_amount(const NutrientFlow& flow, const float rate) const
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

void Tendon::transfer_nutrients(Cell& cell_a, Cell& cell_b)
{
	const float rate = std::max(genome.nutrient_transfer_rate, 0.0f);
	if (rate <= 0.0f)
		return;

	const NutrientFlow flow = pick_nutrient_flow(cell_a, cell_b);
	const float send_amount = calc_nutrient_send_amount(flow, rate);

	*flow.sender -= send_amount;
	*flow.receiver += send_amount * (1.0f - nutrients_transfer_loss);
}


float Tendon::target_extension_fraction()
{
	const float sin_value = fast_sin(phase_ + genome.offset);          // [-1, 1]
	extension_ratio = genome.vertical_shift + genome.amplitude * sin_value; // unclamped
	return std::clamp(extension_ratio, 0.f, 1.f);
}

float Tendon::get_spring_constant() const
{
	const float growth = std::clamp(static_cast<float>(internal_clock_) / fully_developed_age, 0.f, 1.f);
	return genome.spring_const * growth;
}