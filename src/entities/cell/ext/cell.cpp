#include "../cell.h"

void Cell::reset()
{
	energy = initial_energy;
	clock_ = 0;
	generation = 0;
	time_since_last_ate_ = 0;
	nutrients_ = 0.f;
	total_food_eaten_ = 0;
	stomach_ = 0;
	frames_alive_ = 0;
	integrity = integrity;
	offspring_count = 0;

	reproduce = false;
	offspring_index = -1;
	connection_index = -1;
	spring_to_copy_index = -1;
	energy = initial_energy;

	velocity_ = { 0, 0 };

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

void  Cell::accelerate(const sf::Vector2f acceleration)
{
	velocity_ += acceleration;
}


void  Cell::create_offspring(Cell* child, const bool mutate)
{
	// dormant means the child cell will not tug or pull on the protozoa
	// if dormant is false the child will take on the mutations of the parent
	// if mutate is true then the child will go through mutation 

	// When creating an offspring, this is ran for every cell in the protozoa
	child->position_ = get_pos_nearby(2.f);

	time_since_last_reproduced_ = 0;
	offspring_count++;
	child->generation++;

	energy -= offspring_energy_cost;

	child->copy_genetics(*this);

	if (mutate)
		child->mutate();
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


// When a child cell is created, it should be spawned somewhere near the parent cell.
[[nodiscard]] sf::Vector2f  Cell::get_pos_nearby(const float range) const
{
	// Range in terms of radii
	const sf::FloatRect spawn_area = {
	{position_.x - radius * range, position_.y - radius * range},
	{radius * range * 2 , radius * range * 2}
	};
	return Random::rand_pos_in_rect(spawn_area);
}

[[nodiscard]] float  Cell::calculate_friction() const
{
	const float sin_value = sinf(frequency * clock_ + offset); // [-1, 1]
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
}


void  Cell::update_organics()
{
	sinwave_current_friction_ = calculate_friction();

	convert_nutrients_to_energy();
	convert_nutrients_to_integrity();
	energy -= energy_decay_rate;

	if (energy > reproduce_energy_thresh)
		reproduce = true;
}

void  Cell::update_physics()
{

	// updating the velocity with the new friction
	velocity_ *= sinwave_current_friction_;

	position_ += velocity_;
}

void  Cell::convert_nutrients_to_integrity()
{
	if (energy < integrity_conversion_rate)
		return;

	integrity += integrity_conversion_rate;
	energy -= integrity_conversion_rate;
}


void  Cell::convert_nutrients_to_energy()
{
	if (nutrients_ < conversion_rate)
		return;

	energy += conversion_rate;
	nutrients_ -= conversion_rate;
}