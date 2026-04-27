#include "../Protozoa.h"
#include "../../Utils/random.h"

#include "../../Food/food_manager.h"

inline static constexpr  size_t cell_positions_container_reserve = 30;
inline static constexpr  size_t food_positions_container_reserve = 30;


Protozoa::Protozoa(const int id_, Circle* world_bounds, sf::RenderWindow* window)
	: m_window_(window), m_world_bounds_(world_bounds), id(id_), GenomeManager(&m_cells_, &m_springs_)
{

	//food_positions_nearby.reserve(cell_positions_container_reserve);
	//cell_positions_nearby.reserve(food_positions_container_reserve);

	// if no world bounds are provided, we cannot initialise cells as we do not know where to spawn them
	if (world_bounds == nullptr)
	{
		std::cerr << "ERROR: Protozoa was created without world bounds, cannot initialise cells.\n";
		std::cerr << "Protozoa ID: " << id_ << " is window nullptr: " << (window == nullptr) << "\n";
	}
}


void Protozoa::update(FoodManager& food_manager, const bool debug, const float min_speed)
{
	if (m_cells_.empty()) // No computation is needed if there are no cells
		return;

	check_death_conditions(min_speed);

	update_springs();

	update_bounding_box();

	update_cells();

	//handle_food(food_manager, debug);
	reproduce_check();

	++frames_alive;

	energy -= energy_decay_rate;
	

	const sf::Vector2f center = get_center();
	//velocity = center - previous_position;
	//previous_position = center;
}


void Protozoa::check_death_conditions(float min_speed)
{
	float sp = min_speed;
	//if (velocity.x * velocity.x + velocity.y * velocity.y < sp * sp)
	//{
	//	kill();
	//}

	if (energy <= 0)
	{
		kill();
	}
}


void Protozoa::update_springs()
{
	// updates the springs connecting the cells
	energy_lost_to_springs = 0.f;
	for (Spring& spring : m_springs_)
	{
		Cell& cell_A = m_cells_[spring.cell_A_id];
		Cell& cell_B = m_cells_[spring.cell_B_id];
		float energy_expendage = spring.update(cell_A, cell_B, frames_alive);
		energy -= energy_expendage;
		energy_lost_to_springs += energy_expendage;

		if (spring.broken)
		{
			kill();
			return;
		}
	}
}

void Protozoa::update_cells()
{
	// updates each cell in the organism
	for (Cell& cell : m_cells_)
	{
		cell.update(frames_alive);

		if (cell_wander_check(cell))
		{
			kill();
			return;
		}

		energy += cell.nutrients_eaten;
		cell.nutrients_eaten = 0.f;
		stomach += cell.food_eaten;
		cell.food_eaten = 0;
	}
}

void Protozoa::bound_cells()
{
	// updates each cell in the organism
	for (Cell& cell : m_cells_)
	{
		cell.bound(*m_world_bounds_);
	}
}


void Protozoa::update_bounding_box()
{
	// Calculates the bounding box of the protozoa by finding the outer cells coordinates
	const sf::Vector2f cell0_pos = m_cells_[0].position_;
	const float radius = m_cells_[0].radius;

	float min_x = cell0_pos.x - radius;
	float min_y = cell0_pos.y - radius;
	float max_x = cell0_pos.x + radius;
	float max_y = cell0_pos.y + radius;

	// we check over every cell to see if it beats the current bounds
	for (const Cell& cell : m_cells_) 
	{
		const sf::Vector2f pos = cell.position_;
		min_x = std::min(min_x, pos.x - radius);
		max_x = std::max(max_x, pos.x + radius);
		min_y = std::min(min_y, pos.y - radius);
		max_y = std::max(max_y, pos.y + radius);
	}

	const float width = max_x - min_x;
	const float height = max_y - min_y;

	m_personal_bounds_ = { {min_x, min_y}, {width, height} };
}

void Protozoa::move_center_location_to(const sf::Vector2f new_center)
{
	const sf::Vector2f center = get_center();
	const sf::Vector2f translation = new_center - center;
	for (Cell& cell : m_cells_)
	{
		cell.position_ += translation;
	}
	update_bounding_box();
}
sf::Vector2f Protozoa::get_center() const
{
	return m_personal_bounds_.position + m_personal_bounds_.size / 2.f;
}

void Protozoa::soft_reset()
{
	frames_alive = 0.f;
	dead = false;
	reproduce = false;

	stomach = 0;
	total_food_eaten = 0;
	offspring_count = 0;
	energy = initial_energy;
	immortal = false;

	time_since_last_reproduced = 0;

	for (Cell& cell : m_cells_)
	{
		cell.reset();
	}
}

void Protozoa::hard_reset()
{
	soft_reset();

	// setting the containers back to zero size, for when we are restarting the simulation
	m_cells_.clear();
	m_springs_.clear();

	m_personal_bounds_ = { {0.f, 0.f}, {0.f, 0.f } };

	// position and velocity tracking
	//previous_position = { 0, 0 };
	//velocity = { 0, 0 };
	birth_location = { 0, 0 };


	//food_positions_nearby.clear();
	//cell_positions_nearby.clear();

	active = true; // for o_vector.h
}

void Protozoa::set_protozoa_attributes(Protozoa* other)
{
	m_cells_ = other->m_cells_;
	m_springs_ = other->m_springs_;

	update_bounding_box();
}

void Protozoa::init_one_cell()
{
	m_cells_.clear();
	m_springs_.clear();

	m_cells_.emplace_back(0, m_world_bounds_->rand_pos());
}

void Protozoa::resolve_collisions(const std::vector<sf::Vector2f>& collision_resolutions, int& idx)
{
	for (int cell_idx = 0; cell_idx < m_cells_.size(); ++cell_idx)
	{
		Cell& cell = m_cells_[cell_idx];
		cell.position_ += collision_resolutions[idx++];
	}
}


void Protozoa::copy_protozoa_data(Protozoa& dst, const Protozoa& src)
{
	// identity / stats
	dst.id = src.id;
	dst.active = src.active;
	dst.time_since_last_reproduced = src.time_since_last_reproduced;
	dst.birth_location = src.birth_location;
	dst.energy_lost_to_springs = src.energy_lost_to_springs;
	dst.offspring_count = src.offspring_count;
	dst.frames_alive = src.frames_alive;
	dst.total_food_eaten = src.total_food_eaten;
	//dst.previous_position = src.previous_position;
	//dst.velocity = src.velocity;
	dst.immortal = src.immortal;
	//dst.food_positions_nearby = src.food_positions_nearby;
	//dst.cell_positions_nearby = src.cell_positions_nearby;
	
	dst.m_window_ = src.m_window_;
	dst.m_world_bounds_ = src.m_world_bounds_;

	// genome / components
	dst.m_cells_ = src.m_cells_;
	dst.m_springs_ = src.m_springs_;

	// internal state
	dst.stomach = src.stomach;
	dst.energy = src.energy;
	dst.reproduce = src.reproduce;
	dst.dead = src.dead;

	dst.m_personal_bounds_ = src.m_personal_bounds_;

	// NOTE: m_window_ and m_world_bounds_ are intentionally NOT copied.
	// Each protozoa holds a pointer to the world's resources, which are
	// assigned once at construction and must not change.
}


Protozoa::Protozoa(const Protozoa& other)
	: ProtozoaSettings(),
	GenomeManager(&m_cells_, &m_springs_),
	m_window_(other.m_window_),
	m_world_bounds_(other.m_world_bounds_)
{
	copy_protozoa_data(*this, other);
}

Protozoa& Protozoa::operator=(const Protozoa& other)
{
	if (this == &other) return *this;
	copy_protozoa_data(*this, other);
	// pointers stay as-is — don't adopt another protozoa's window/bounds
	return *this;
}