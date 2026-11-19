#include "cell.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <entities/body/body.h>
#include <random>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>
#include <Utils/random.h>
#include <Utils/oscillator.h>


void Cell::recreate()
{
	// Energy, Integrity, And Nutrients
	energy = initial_energy;
	delta_energy = 0.f;

	integrity = initial_integrity;
	delta_integrity = 0.f;

	nutrients_ = 0.f;
	phase_ = 0.f;

	// genome
	reset_generation();

	// stats
	internal_clock_ = 0;
	time_since_last_ate_ = 0;
	total_food_eaten_ = 0;
	offspring_count = 0;

	cumulative_collision_damage_ = 0.f;
	cumulative_tendon_damage_ = 0.f;

	new_connections_made = uint8_t(0);

	

	repro_timer_ = 0;

	nearby_food_ids_size_ = 0;

	randomize();
	tendon_genome.randomize();   // <-- clears stale genome from recycled pool slot

	mass_curve_ceiling = get<Mass>();
	growth_rate = get<InitialGrowthRate>();
	set_max_energy_nut_integrity();

	reproduce_ = false;
	dead_ = false;
	immortal_ = false;
	seek_newborn_links_ = false;

	tendon_links_.clear_connections();
}


bool  Cell::consume_food_check(const sf::Vector2f& cell_pos, const sf::Vector2f& food_pos, const float combined_rad)
{
	return (food_pos - cell_pos).lengthSquared() < combined_rad * combined_rad;
}

sf::Vector2f Cell::get_pos_nearby_min_max(const sf::Vector2f parent_pos, float min_mass, float max_mass)
{
	static thread_local std::mt19937 rng{ Random::get_engine() };
	constexpr float inv_range = 1.0f / 4294967295.0f; // 1 / 2^32-1
	constexpr float two_pi = 6.28318530718f;

	const float angle = (rng() * inv_range) * two_pi;
	const float radius = min_mass + (rng() * inv_range) * (max_mass - min_mass);

	return parent_pos + sf::Vector2f(std::cos(angle) * radius, std::sin(angle) * radius);
}

void Cell::create_offspring(Body* this_body, Cell* child, Body* child_body, const bool mutate)
{
	// This function takes in blank child and child_body data and creates an offspring with it

	child_body->copy(this_body); // copies the body 
	float min_r = this_body->get_radius() + child_body->get_radius() + 1.f; // minimum distance between the two cells
	child_body->set_pos(get_pos_nearby_min_max(this_body->get_pos(), min_r, min_r * 2.f)); // moves the position to somewhere near the parent

	// Updating this cell's (parent)'s statistics
	repro_timer_ = 0;
	offspring_count++;


	// a fraction of the energy of the parent is given to the child
	const float give = std::clamp(get<OffspringEnergyToGive>(), 0.1f, 0.9f);
	const float total_energy = energy;
	energy = total_energy * (1.f - give);
	child->energy = total_energy * give;

	// Finally the genetics, we copy the cell and tendon genetics and only mutate them if permissable
	child->copy(*this);
	child->tendon_genome.copy(tendon_genome);

	if (mutate)
	{
		child->mutate();
		child->tendon_genome.mutate();
	}

	// The mass is an evolutionary paramater that needs to be told to the body
	child->apply_to_body(child_body);
	child->set_init_mass_curve_ceiling();

	child->phase_ = phase_; // newborn starts in step with its parent

	child->set_max_energy_nut_integrity();
}

void Cell::set_init_mass_curve_ceiling()
{
	mass_curve_ceiling = get<Mass>();
	growth_rate = get<InitialGrowthRate>();
}

void Cell::turn_off_reproduction()
{
	reproduce_ = false;
}


// When a child cell is created, it should be spawned somewhere near the parent cell.
[[nodiscard]] sf::Vector2f  Cell::get_pos_nearby(const Body* body, const float range) const
{
	// Range in terms of radii
	const sf::Vector2f pos = body->get_pos();
	const float radius = body->get_radius();
	return Random::rand_pos_in_rect(sf::FloatRect{
		{ pos.x - radius * range, pos.y - radius * range },
		{ radius * range * 2 , radius * range * 2 }
		});
}

[[nodiscard]] float Cell::calculate_sin_output() const
{
	const float sin_value = fast_sin(phase_ + get<Offset>()); // [-1, 1]
	const float ratio = get<VerticalShift>() + get<Amplitude>() * sin_value;     // [vs-a, vs+a]
	const float clamped = std::clamp(ratio, 0.f, 1.f); // keeping the output between 0 and 1, so that the drag coefficient is always valid

	return clamped;
}

bool Cell::should_try_connect_to_others() const
{
	bool is_newborn = internal_clock_ < infant_time;
	bool is_check_frame = internal_clock_ % infant_check_interval == 0;
	bool is_first_gen = generation() == 0;
	bool can_have_more_connections = new_connections_made < TendonLinks::max_connections;

	return is_newborn && is_check_frame && !is_first_gen && can_have_more_connections && seek_newborn_links_;
}

void Cell::update_statistics()
{
	internal_clock_++;
	if (time_since_last_ate_ != UINT16_MAX) time_since_last_ate_++;
	if (repro_timer_ != UINT16_MAX)         repro_timer_++;
}

float Cell::drag_coeff() const 
{
	return CellSettings::drag_min + sinwave_current_value_ * (CellSettings::drag_max - CellSettings::drag_min);
}

void Cell::update_organics(bool immune, bool friction_energy_loss, bool old_age_death)
{
	phase_ = osc::wrap_two_pi(phase_ + get<Frequency>());
	sinwave_current_value_ = calculate_sin_output();
	cell_calculated_drag_ = drag_coeff();

	if (immune)
		apply_immunity();
	if (friction_energy_loss)
		apply_passive_decay();
	if (old_age_death)
		apply_old_age_decay();

	process_nutrients();

	check_death();

	repair_integrity();
	update_reproduction_flag();
	atrophy();

	flush_deltas();
}

void Cell::apply_immunity()
{
	integrity = max_integrity; // was hardcoded 100.f — desyncs if max_integrity is ever tuned
}

void Cell::apply_passive_decay()
{
	/* when a cell grips onto the world it costs energy */
	delta_energy -= sinwave_current_value_ * friction_energy_loss_const;
}

void Cell::apply_old_age_decay()
{
	/* when a cell reaches its maximum age, it starts to decay based on how far past its death age it is */
	if (internal_clock_ <= death_age_threshold)
		return;

	const float ticks_over = static_cast<float>(internal_clock_ - death_age_threshold);
	delta_integrity -= death_integrity_rate * (1.f + ticks_over * death_decay_growth);
}


void Cell::check_death()
{
	/* Check if the cell is dead or should die */
	if (integrity <= 0.f) // if the cell has no integrity, it dies
		kill();
}

void Cell::update_reproduction_flag()
{
	/* Check if the cell is ready to reproduce */
	reproduce_ = check_sufficient_energy()
		&& check_sufficient_integrity()
		&& check_sufficient_nutrients()
		&& check_repro_cooldown();
}

void Cell::grow(Body* body)
{
	static const float growth_cost = .3f;
	static const float rate_cap = 2.f;

	float mass = body->get_mass();

	mass_curve_ceiling += growth_rate;
	growth_rate = std::max(0.f, growth_rate - get<GrowthDecay>());

	float wanted = mass_curve_ceiling - mass;
	float affordable = energy / growth_cost;
	float delta_mass = std::max(0.f, std::min({ wanted, rate_cap, affordable }));

	if (delta_mass > 0)
	{
		body->add_mass(delta_mass);
		energy -= delta_mass * growth_cost;

		set_max_energy_nut_integrity();
		body->set_radius();
	}
}

// ------------------------------------- ENERGY , NUTRIENTS, INTEGRITY -------------------------------------
void Cell::process_nutrients()
{
	/* convert nutrients to energy */
	const float energy_capacity = max_energy - energy;

	// no nutrients to convert or no room for energy
	if (nutrients_ <= 0.f || energy_capacity <= 0.f) 
		return;

	// Convert nutrients to energy, but don't exceed the conversion rate or the energy capacity
	const float amount = std::min({ nutrients_, nutrients_conversion_rate, energy_capacity });
	delta_energy += amount;
	nutrients_ -= amount;
}

void Cell::repair_integrity()
{
	/* convert energy to integrity */
	if (integrity >= max_integrity || energy < energy_repair_rate)
		return;

	delta_energy -= energy_repair_rate;
	delta_integrity += energy_repair_rate;
}

// Applies accumulated change_energy()/change_integrity() calls from this tick
// (tendons, impulse damage, etc.) in one clamped step.
void Cell::flush_deltas()
{
	energy = std::clamp(energy + delta_energy, 0.f, max_energy);
	delta_energy = 0.f;

	integrity = std::clamp(integrity + delta_integrity, 0.f, max_integrity);
	delta_integrity = 0.f;

	nutrients_ = std::clamp(nutrients_, 0.f, max_nutrients);
}

// This function sets the maximum energy, nutrients, and integrity of the cell based on its mass. It is called whenever the cell's mass changes (e.g., when it grows or shrinks).
void Cell::set_max_energy_nut_integrity()
{
	max_nutrients = get<Mass>() * mass_to_max_nutrients;
	max_integrity = get<Mass>() * mass_to_max_integrity;
	max_energy = get<Mass>() * mass_to_max_energy;
}

bool Cell::eat(const float nutrients)
{
	// This function returns false if the cell has failed to eat the food
	// and true if it has eaten the food
	if (time_since_last_ate_ < digestive_time || nutrients_ + nutrients > max_nutrients)
		return false;

	nutrients_ += nutrients;
	time_since_last_ate_ = 0;
	++total_food_eaten_;

	return true;
}

void Cell::atrophy()
{
	if (energy > 0.f) // if the cell has no energy, it dies
		return;
	
	static constexpr float atrophy_rate = 0.3f; // how much integrity is lost for every 1 energy lost

	integrity -= atrophy_rate;
}