#pragma once
#include <algorithm>

#include "../Protozoa/Protozoa.h"
#include "../Utils/o_vector.hpp"
#include "../Food/food_manager.h"
#include <SFML/Graphics.hpp>


/* How reproduction works, in detail
when a cell has enough energy its sets reproduce = true
the protozoa manager detects this and
- cell.reproduce = false;
- makes a create cell request

The birth manager then processes this request by
- creating a new cell
- cell.offspring_index = the index of the new cell
- a temporary spring is made between the parent cell and the new cell

This keeps happening until a spring detects that both its cell's have a valid offspring index
The spring then
- sets cell_a.connection_index = cell_b.offspring_index; 
- cell_a.spring_to_copy_index = id; this is the spring data that will be used
This tells the protozoa manager what to connect (cell_a.connection_index, cell_b.offspring_index)

The connection request is detected
 */


struct BirthRequest
{
	uint8_t parent_cell_id;
};

struct ConnectionRequest
{
	uint8_t offspring_id;
	uint8_t connect_to_id;
	int8_t  spring_to_copy_index;
};

// A Class which handles all protozoa related stuff in the world. updating, collisions, reproduction, etc.
class ProtozoaManager : protected WorldSettings
{
public:
	sf::RenderWindow* m_window_ = nullptr;

	o_vector<Protozoa> all_protozoa_;

	uint16_t   longest_lived_ever_ = 0;
	uint8_t   most_offspring_ever_ = 0;
	float infant_mortality_rate_ = 0.f;
	int   total_deaths_ = 0;
	int   infant_deaths_ = 0;
	
public:
	// The user can click on a protozoa to select it for debugging purposes. we store a pointer to it here.
	Protozoa* selected_protozoa_ = nullptr;

	float average_lifetime_ = 0.f; // the average lifetime of the 500 most recent protozoa deaths

	std::vector<float> recent_lifetimes_ = {}; // a vector storing the lifetimes of the 500 most recent protozoa deaths, used to calculate average_lifetime_

	static constexpr size_t max_lifetime_samples_ = 500;

	int deaths_this_window_ = 0;
	int births_this_window_ = 0;
	static constexpr int survival_rate_window_size_ = 100;

	std::vector<BirthRequest> birth_requests;
	std::vector<ConnectionRequest> connection_requests;

	// every frame this is filled with the collision resolutions calculated in the collision detection phase, and then applied to the cells in the update phase. 
	// this is done to avoid modifying cell velocities during the collision detection phase which can cause errors in subsequent collision checks within the same frame.
	alignas(64) std::vector<sf::Vector2f> collision_resolutions{};

	std::vector<int> reproduce_indexes{};

	ProtozoaManager(sf::RenderWindow* window) : m_window_(window), all_protozoa_(max_protozoa)
	{
		birth_requests.reserve(10);
		connection_requests.reserve(10);
		reproduce_indexes.reserve(max_protozoa);

		std::cout << "[INFO]: ProtozoaManager initialized with max protozoa: " << max_protozoa << "\n";
		if (window == nullptr)
		{
			std::cerr << "[ERROR]: ProtozoaManager initialized with null window pointer.\n";
		}
	}

	void inject_protozoa(Protozoa* protozoa, const float energy_injected)
	{
		const auto cell_count = static_cast<float>(protozoa->get_cells().size());
		for (Cell& cell : protozoa->get_cells())
		{
			cell.energy += energy_injected / cell_count;
		}
	}

	void soft_reset_protozoa(Protozoa* protozoa)
	{
		for (Cell& cell : protozoa->get_cells())
		{
			cell.reset();
		}
	}

	void hard_reset_protozoa(Protozoa* protozoa)
	{
		soft_reset_protozoa(protozoa);

		protozoa->get_cells().clear();
		protozoa->get_springs().clear();


		protozoa->active = true; // for o_vector.h
	}


	Protozoa* get_selected_protozoa() const { return selected_protozoa_; }

	int get_protozoa_count() const
	{
		return all_protozoa_.size();
	}

	float calculate_average_generation() const
	{
		if (all_protozoa_.size() == 0) 
			return 0.f;

		float sum = 0.f;
		int count = 0;
		for (Protozoa* protozoa : all_protozoa_)
		{
			for (Cell& cell : protozoa->get_cells())
			{
				sum += cell.generation;
				count++;
			}
		}
		return sum / count;
	}


	void deselect_protozoa()
	{
		if (selected_protozoa_ != nullptr)
		{
			selected_protozoa_ = nullptr;
		}
	}


	inline static constexpr int max_evolutionary_iterations = 5;
	inline static constexpr int desired_cell_count = 3;


	Protozoa* find_protozoa_by_id(const int id)
	{
		return all_protozoa_.at(id);
	}


	void create_offspring(Protozoa* parent, const bool should_mutate = true)
	{
		//Protozoa* offspring = get_unallocated_protozoa();
	}

protected:
	inline void build_protozoa(Protozoa& protozoa, const Circle& world_bounds, bool emplace_back = true)
	{
		// To build a protozoa, we generate a random amount of cells and then run a spring connection algorithm for a random amount of iterations
		hard_reset_protozoa(&protozoa);

		std::vector<Cell>& cells = protozoa.get_cells();
		sf::Vector2f spawn_position = world_bounds.rand_pos();
		float spawn_dist = 200.f;
		sf::FloatRect spawn_rect = { spawn_position - sf::Vector2f(spawn_dist, spawn_dist), sf::Vector2f(spawn_dist * 2, spawn_dist * 2) };
		const int cell_count = Random::rand_range(2, desired_cell_count);
		const int spring_count = Random::rand_range(cell_count-1, cell_count * 2);

		// creating the cells
		for (int i = 0; i < cell_count; ++i)
			cells.emplace_back(i, Random::rand_pos_in_rect(spawn_rect));

		// Now we create springs between the cells
		for (int i = 0; i < spring_count; ++i)
		{
			if (cells.size() < 2)
				return;

			// choose two different cell ids
			int cell_A_id = Random::rand_range(size_t(0), cells.size() - 1);
			int cell_B_id = Random::rand_range(size_t(0), cells.size() - 1);

			if (cell_A_id == cell_B_id)
				return; // better luck next time

			// check if a spring already exists between these two cells, if so we don't add another one
			for (const Spring& spring : protozoa.get_springs())
			{
				if ((spring.cell_A_id == cell_A_id && spring.cell_B_id == cell_B_id) ||
					(spring.cell_A_id == cell_B_id && spring.cell_B_id == cell_A_id))
				{
					return; // spring already exists
				}
			}

			const auto spring_id = static_cast<int>(protozoa.get_springs().size());
			protozoa.get_springs().emplace_back(spring_id, cell_A_id, cell_B_id);
		}
	}

	void update_all_protozoa(bool track_statistics)
	{
		reproduce_indexes.clear();

		for (Protozoa* protozoa : all_protozoa_)
		{
			protozoa->update();

			birth_requests.clear();
			connection_requests.clear();

			collect_reproduction_requests(
				protozoa->get_cells());

			apply_birth_requests(protozoa->get_cells(), protozoa->get_springs());

			apply_connection_requests(protozoa->get_cells(), protozoa->get_springs());

			if (!check_death_conditions(protozoa))
				continue;

			if (track_statistics)
			{
				for (Cell& cell : protozoa->get_cells())
				{
					register_death_stat(cell.frames_alive_, cell.offspring_count > 0);
				}
			}
			all_protozoa_.remove(protozoa);
		}

		const int passes = 100;
		Protozoa* new_protozoa = all_protozoa_.add();
		for (Protozoa* current : all_protozoa_)
		{
			if (new_protozoa == nullptr)
				break;

			if (current->run_separation(new_protozoa))
			{
				new_protozoa = all_protozoa_.add();
			}
		}

		for (const int idx : reproduce_indexes)
		{
			create_offspring(all_protozoa_.at(idx));
			if (track_statistics)
				register_birth_stat();
		}
	}
	

	void update_cell_collisions() const
	{
		int idx = 0;
		
		for (Protozoa* protozoa : all_protozoa_)
		{
			for (auto& cell : protozoa->get_cells())
			{
				cell.accelerate(collision_resolutions[idx++]);
			}
		}
	}

	Protozoa* get_unallocated_protozoa()
	{
		// This function will either return an unallocated protozoa from the pool, or if none are available, 
		// it will return a random existing protozoa to be overwritten.
		Protozoa* offspring = all_protozoa_.add();

		if (offspring == nullptr)
		{
			const size_t idx = Random::rand_range(unsigned(0), all_protozoa_.size() - 1);
			offspring = all_protozoa_.at(idx);
		}
		return offspring;
	}

	
	bool reproduce_check(Protozoa* protozoa)
	{
		for (Cell& cell : protozoa->get_cells())
		{
			if (!cell.can_reproduce())
			{
				return false;
			}
		}

		return true;
	}

	void check_for_extinction_event(Circle& world_bounds)
	{
		// if protozoas are still alivee or if auto reset on extinction is disabled, we dont need to do anything
		if (all_protozoa_.size() > 0 || !auto_reset_on_extinction)
			return;

		std::cout << "Extinction event occurred, respawning initial protozoa...\n";

		for (unsigned i = 0; i < initial_protozoa; ++i)
		{
			Protozoa* protozoa = all_protozoa_.add();
			build_protozoa(*protozoa, world_bounds, false);
		}

		
		
	}

	void register_death_stat(const float lifetime, const bool had_offspring)
	{
		recent_lifetimes_.push_back(lifetime);
		if (recent_lifetimes_.size() > max_lifetime_samples_)
			recent_lifetimes_.erase(recent_lifetimes_.begin());

		float sum = 0.f;
		for (float l : recent_lifetimes_) sum += l;
		average_lifetime_ = recent_lifetimes_.empty() ? 0.f : sum / recent_lifetimes_.size();

		deaths_this_window_++;
		++total_deaths_;
		if (!had_offspring) ++infant_deaths_;
		infant_mortality_rate_ = total_deaths_ > 0
			? static_cast<float>(infant_deaths_) / total_deaths_ : 0.f;

		longest_lived_ever_ = std::max(static_cast<uint16_t>(lifetime), longest_lived_ever_);
	}

	void register_birth_stat()
	{
		births_this_window_++;
	}

	void init_protozoa_container(Circle& world_bounds)
	{
		// We create the maximum amount of protozoa at the start
		for (int i = 0; i < max_protozoa; ++i)
			all_protozoa_.emplace({ i });

		// removing any protozoa that are above the initial protozoa count
		for (int i = initial_protozoa; i < max_protozoa; ++i)
		{
			soft_reset_protozoa(all_protozoa_.at(i));
			all_protozoa_.remove(i);
		}

		for (Protozoa* protozoa : all_protozoa_)
		{
			build_protozoa(*protozoa, world_bounds);
		}
	}

	


	[[nodiscard]] static bool check_death_conditions(Protozoa* protozoa)
	{
		for (Cell& cell : protozoa->get_cells())
		{
			if (!cell.can_die())
			{
				return false;
			}
		}
		return true; // protozoa is only dead if all its cells are dead
	}


private:
	void collect_reproduction_requests(std::vector<Cell>& cells)
	{
		const int cell_count = static_cast<int>(cells.size());

		for (int i = 0; i < cell_count; ++i)
		{
			Cell& cell = cells[i];

			if (cell.reproduce)
			{
				cell.reproduce = false;
				birth_requests.push_back({ cell.id });

				if (Random::rand01_float() > 0.01)
					break;
			}
			else if (cell.spring_to_copy_index >= 0)
			{
				connection_requests.push_back({
					static_cast<uint8_t>(cell.offspring_index),
					static_cast<uint8_t>(cell.connection_index),
					cell.spring_to_copy_index
					});
				cell.connection_index = -1;
				cell.offspring_index = -1;
				cell.spring_to_copy_index = -1;

				if (Random::rand01_float() > 0.01)
					break;
			}
		}
	}

	void apply_birth_requests(std::vector<Cell>& cells, std::vector<Spring>& springs)
	{
		cells.reserve(cells.size() + birth_requests.size());
		springs.reserve(springs.size() + birth_requests.size());

		for (const BirthRequest& req : birth_requests)
		{
			const uint8_t offspring_id = static_cast<uint8_t>(cells.size());

			cells.emplace_back();
			Cell* offspring = &cells.back();         
			cells[req.parent_cell_id].create_offspring(offspring);
			offspring->id = offspring_id;

			// it's important to tell the parent cell which offspring is theirs, so we can apply connection requests
			cells[req.parent_cell_id].offspring_index = offspring_id;

			// when we create this offspring we create a spring to it, the spring has a weak connection as it is made to break
			// This is a temporary spring, it needs hold the new cell close to the parent cell until the real spring is made
			// this is because if the two new cells are too far apart when the spring is made, the spring will break immediately and the offspring will die before it can reproduce

			const uint8_t spring_id = static_cast<uint8_t>(springs.size());
			springs.emplace_back(spring_id, req.parent_cell_id, offspring_id);
			springs.back().spring_const = 0.00001f;
			springs.back().amplitude = 0.f;
			springs.back().damping = 0.9f;

			const sf::Vector2f diff = cells[req.parent_cell_id].get_pos() - offspring->get_pos();
			springs.back().rest_length = diff.length();
		}
	}

	void apply_connection_requests(std::vector<Cell>& cells, std::vector<Spring>& springs)
	{
		springs.reserve(springs.size() + connection_requests.size());

		for (const ConnectionRequest& req : connection_requests)
		{
			// we need to check if the spring to copy index is valid, if it is we copy the spring data to the new spring
			const bool valid_spring_to_copy = req.spring_to_copy_index >= 0
				&& req.spring_to_copy_index < static_cast<int8_t>(springs.size());

			const uint8_t spring_id = static_cast<uint8_t>(springs.size());
			springs.emplace_back(spring_id, req.offspring_id, req.connect_to_id);
			Spring* new_spring = &springs.back();

			if (valid_spring_to_copy)
			{
				springs[req.spring_to_copy_index].create_offspring(*new_spring);
			}
			
		}
	}
};