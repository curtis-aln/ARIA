#include "../cell.h"



void Cell::recreate()
{
	energy = initial_energy;
	clock_ = 0;
	generation = 0;
	time_since_last_ate_ = 0;
	nutrients_ = 0.f;
	total_food_eaten_ = 0;
	stomach_ = 0;
	frames_alive_ = 0;
	integrity = 100;
	offspring_count = 0;
	frames_since_offspring_pending_ = 0;

	reproduce = false;
	offspring_index = -1;
	connection_index = -1;
	spring_to_copy_index = -1;

	dead = false;
	immortal = false;
}

void Cell::eat(const float nutrients)
{
	nutrients_ += nutrients;
	//stomach_++; ToDO implement stomach

	time_since_last_ate_ = 0;
	++total_food_eaten_;
}

bool  Cell::consume_food_check(const sf::Vector2f& cell_pos, const sf::Vector2f& food_pos, const float combined_rad)
{
	return (food_pos - cell_pos).lengthSquared() < combined_rad * combined_rad;
}


void Cell::create_offspring(Cell* child, Body* parent_body, Body* child_body, const bool mutate)
{
	// dormant means the child cell will not tug or pull on the protozoa
	// if dormant is false the child will take on the mutations of the parent
	// if mutate is true then the child will go through mutation 

	// When creating an offspring, this is ran for every cell in the protozoa
	child_body->mass_ = parent_body->mass_;
	child_body->radius_ = parent_body->radius_;

	child_body->position_ = get_pos_nearby(parent_body, 2.f);

	time_since_last_reproduced_ = 0;
	offspring_count++;
	child->generation++;

	energy /= 2;
	child->energy /= 2;

	child->copy_genetics(*this);

	if (mutate)
		child->mutate();

	// setting the physical parameters of the childs body
	child_body->radius_ = child->radius;

}

[[nodiscard]] bool  Cell::can_die() const
{
	if (energy <= 0 || integrity <= 0)
	{
		return true;
	}
	return false;
}

[[nodiscard]] bool  Cell::can_reproduce() const
{
	if (energy < reproduce_energy_thresh)
		return false;

	if (time_since_last_reproduced_ < reproductive_cooldown)
		return false;

	return true;
}

void Cell::turn_off_reproduction()
{
	time_since_last_reproduced_ = 0;
	reproduce = false;
}


// When a child cell is created, it should be spawned somewhere near the parent cell.
[[nodiscard]] sf::Vector2f  Cell::get_pos_nearby(const Body* body, const float range) const
{
	// Range in terms of radii
	return Random::rand_pos_in_rect(sf::FloatRect{
		{body->position_.x - radius * range, body->position_.y - radius * range},
		{radius * range * 2 , radius * range * 2}
	});
}

[[nodiscard]] float Cell::calculate_friction() const
{
	const float sin_value = fast_sin(frequency * clock_ + offset); // [-1, 1]
	const float ratio = vertical_shift + amplitude * sin_value;     // [vs-a, vs+a]
	const float clamped = std::clamp(ratio, 0.f, 1.f);
	// clamping friction to [0, 1]
	return clamped;
}

void  Cell::update_statistics()
{
	clock_++;
	time_since_last_ate_++;
	frames_alive_++;
	time_since_last_reproduced_++;

	if (offspring_index >= 0)
		frames_since_offspring_pending_++;
	else
		frames_since_offspring_pending_ = 0;
}


void Cell::update_organics(Body* body)
{
	if (dead)
	{
		sinwave_current_friction_ = 0.6f;
		integrity -= integrity_drain_rate;
		body->velocity_ *= sinwave_current_friction_;
		return;
	}

	sinwave_current_friction_ = calculate_friction();
	body->velocity_ *= sinwave_current_friction_;

	// 1. Passive decay — base cost of being alive
	energy -= energy_decay_rate;

	// 2. Digest nutrients → energy, BEFORE the death check
	//    so a fed cell can survive a decay tick it otherwise couldn't
	process_nutrients();

	// 3. Death check — no energy left
	if (energy <= 0.f)
	{
		energy = 0.f;
		dead = true;
		integrity = std::max(0.f, integrity - integrity_drain_rate);
		return;  // dead cells don't repair or reproduce
	}

	// 4. Spend energy to repair integrity
	repair_integrity();

	// 5. Flag for reproduction — use assignment so it clears itself
	//    when energy drops back below threshold
	reproduce = (energy >= reproduce_energy_thresh) && (offspring_index < 0);
}


void Cell::process_nutrients()
{
	if (nutrients_ <= 0.f)
		return;

	// Convert a capped batch per tick — don't convert more than we have
	const float converted = std::min(nutrients_, nutrients_conversion_rate);
	energy += converted;
	nutrients_ -= converted;
}


// Renamed from update_energy — the old name was actively confusing
void Cell::repair_integrity()
{
	if (integrity >= 100.f)
		return;

	// Don't spend energy we don't have — avoids draining below 0
	// and triggering the death check on the next frame for free
	if (energy < integrity_conversion_rate)
		return;

	energy -= integrity_conversion_rate;
	integrity = std::min(100.f, integrity + integrity_conversion_rate);
}