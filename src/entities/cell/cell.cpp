#include "cell.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <entities/body.h>
#include <random>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>
#include <Utils/random.h>



void Cell::recreate()
{
	// Energy, Integrity, And Nutrients
	energy = initial_energy;
	delta_energy = 0.f;

	integrity = initial_integrity;
	delta_integrity = 0.f;

	nutrients_ = 0.f;

	// genome
	generation = 0;

	// stats
	internal_clock_ = 0;
	time_since_last_ate_ = 0;
	total_food_eaten_ = 0;
	offspring_count = 0;

	cumulative_collision_damage_ = 0.f;
	cumulative_spring_damage_ = 0.f;

	new_connections_made = uint8_t(0);

	repro_timer_ = 0;

	nearby_food_ids_size_ = 0;

	randomize();
	spring_genome.randomize();   // <-- clears stale genome from recycled pool slot

	reproduce_ = false;
	dead_ = false;
	immortal_ = false;

	spring_links_.clear_connections();
}

bool Cell::eat(const float nutrients)
{
	// This function returns false if the cell has failed to eat the food
	// and true if it has eaten the food

	if (time_since_last_ate_ < digestive_time || nutrients_ >= max_nutrients)
		return false;

	nutrients_ += nutrients;
	nutrients_ = std::min(nutrients_, max_nutrients);

	time_since_last_ate_ = 0;
	++total_food_eaten_;

	return true;
}

bool  Cell::consume_food_check(const sf::Vector2f& cell_pos, const sf::Vector2f& food_pos, const float combined_rad)
{
	return (food_pos - cell_pos).lengthSquared() < combined_rad * combined_rad;
}

sf::Vector2f Cell::get_pos_nearby_min_max(const sf::Vector2f parent_pos, float min_radius, float max_radius)
{
	static thread_local std::mt19937 rng{ std::random_device{}() };
	constexpr float inv_range = 1.0f / 4294967295.0f; // 1 / 2^32-1
	constexpr float two_pi = 6.28318530718f;

	const float angle = (rng() * inv_range) * two_pi;
	const float radius = min_radius + (rng() * inv_range) * (max_radius - min_radius);

	return parent_pos + sf::Vector2f(std::cos(angle) * radius, std::sin(angle) * radius);
}

void Cell::create_offspring(Body* this_body, Cell* child, Body* child_body, const bool mutate)
{
	// This function takes in blank child and child_body data and creates an offspring with it

	child_body->copy(this_body); // copies the body 
	float min_r = this_body->radius_ + child_body->radius_ + 1.f; // minimum distance between the two cells
	child_body->position_ = get_pos_nearby_min_max(this_body->position_, min_r, min_r * 2.f); // moves the position to somewhere near the parent

	// Updating this cell's (parent)'s statistics
	repro_timer_ = 0;
	offspring_count++;

	// Updating the child cell's statistics
	child->generation = generation + 1;

	// a fraction of the energy of the parent is given to the child
	constexpr float energy_split = 0.25f; // fraction the parent retains; offspring gets the rest
	const float total_energy = energy;    // snapshot parent's energy before splitting

	energy = total_energy * energy_split;
	child->energy = total_energy * (1.f - energy_split);

	// Finally the genetics, we copy the cell and spring genetics and only mutate them if permissable
	child->copy_genetics(*this);
	child->spring_genome.copy_genetics(spring_genome);

	if (mutate)
	{
		child->mutate();
		child->spring_genome.mutate();
	}

	// The radius is an evolutionary paramater that needs to be told to the body
	child_body->radius_ = child->radius;

}

void Cell::turn_off_reproduction()
{
	repro_timer_ = 0;
	reproduce_ = false;
}


// When a child cell is created, it should be spawned somewhere near the parent cell.
[[nodiscard]] sf::Vector2f  Cell::get_pos_nearby(const Body* body, const float range) const
{
	// Range in terms of radii
	return Random::rand_pos_in_rect(sf::FloatRect{
		{ body->position_.x - radius * range, body->position_.y - radius * range },
		{ radius * range * 2 , radius * range * 2 }
		});
}

[[nodiscard]] float Cell::calculate_friction() const
{
	const float sin_value = fast_sin(frequency * internal_clock_ + offset); // [-1, 1]
	const float ratio = vertical_shift + amplitude * sin_value;     // [vs-a, vs+a]
	const float clamped = std::clamp(ratio, 0.f, 1.f);
	// clamping friction to [0, 1]
	return clamped;
}

void  Cell::update_statistics()
{
	internal_clock_++;
	time_since_last_ate_++;
	repro_timer_++;
}


void Cell::update_organics(bool immune, bool friction_energy_loss, bool old_age_death)
{
	if (immune)
		apply_immunity();

	if (friction_energy_loss)
		apply_passive_decay();
	process_nutrients();

	if (check_death(old_age_death))
		return; // dead cells don't repair or reproduce

	repair_integrity();
	update_reproduction_flag();

	flush_deltas();
}

void Cell::apply_immunity()
{
	integrity = max_integrity; // was hardcoded 100.f — desyncs if max_integrity is ever tuned
	dead_ = false;
}

void Cell::apply_passive_decay()
{
	sinwave_current_friction_ = calculate_friction();
	delta_energy -= (1.f - sinwave_current_friction_) * friction_energy_loss_const;
}

// Applies accumulated change_energy()/change_integrity() calls from this tick
// (springs, impulse damage, etc.) in one clamped step.
void Cell::flush_deltas()
{
	energy = std::clamp(energy + delta_energy, 0.f, max_energy);
	delta_energy = 0.f;

	integrity = std::clamp(integrity + delta_integrity, 0.f, max_integrity);
	delta_integrity = 0.f;
}

bool Cell::check_death(bool old_age_death)
{
	/* Check if the cell is dead or should die */

	if (energy <= 0.f) // if the cell has no energy, it dies
	{
		energy = 0.f;
		dead_ = true;
	}
	else if (integrity <= 0.f) // if the cell has no integrity, it dies
	{
		integrity = 0.f;
		dead_ = true;
	}
	else if (old_age_death && internal_clock_ > death_age_threshold) // if the cell has been alive for too long, it dies
	{
		energy -= death_energy_rate;
	}

	return dead_;
}

void Cell::update_reproduction_flag()
{
	/* Check if the cell is ready to reproduce */
	reproduce_ = check_sufficient_energy()
		&& check_sufficient_integrity()
		&& check_sufficient_nutrients()
		&& check_repro_cooldown();
}

void Cell::process_nutrients()
{
	/* convert nutrients to energy */
	if (nutrients_ <= 0.f) // no nutrients to convert
	{
		nutrients_ = 0.f;
		return;
	}

	const float energy_capacity = max_energy - energy;
	if (energy_capacity <= 0.f) // no room for energy
	{
		energy = max_energy;
		return;
	}

	// Convert nutrients to energy, but don't exceed the conversion rate or the energy capacity
	const float amount = std::min({ nutrients_, nutrients_conversion_rate, energy_capacity });
	delta_energy += amount;
	nutrients_ -= amount;
}

void Cell::repair_integrity()
{
	/* convert energy to integrity */
	if (integrity >= max_integrity)
		return;

	if (energy < integrity_conversion_rate)
		return; // don't drain below 0 and trigger death for free next frame

	delta_energy -= integrity_conversion_rate;
	delta_integrity += integrity_conversion_rate;
}