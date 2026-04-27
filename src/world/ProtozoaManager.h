#pragma once
#include <algorithm>

#include "../Protozoa/Protozoa.h"
#include "../Utils/o_vector.hpp"
#include "../Food/food_manager.h"

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



	// every frame this is filled with the collision resolutions calculated in the collision detection phase, and then applied to the cells in the update phase. 
	// this is done to avoid modifying cell velocities during the collision detection phase which can cause errors in subsequent collision checks within the same frame.
	alignas(64) std::vector<sf::Vector2f> collision_resolutions{};



	ProtozoaManager(sf::RenderWindow* window) : m_window_(window), all_protozoa_(max_protozoa)
	{

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

	void render_protozoa_springs(const Protozoa* protozoa);
	void render_debug(const Protozoa* protozoa, Font* font, bool skeleton_mode, bool show_connections,
	                  bool show_bounding_boxes);
	void draw_cell_outlines(const Protozoa* protozoa);
	void nearby_food_information(const Protozoa* protozoa) const;
	void render_cell_connections(const Protozoa* protozoa, const Cell& cell, bool thick_lines) const;
	void draw_cell_physics(const Protozoa* protozoa, Font* font);
	void draw_spring_information(const Protozoa* protozoa, Font* font) const;
	int check_mouse_press(const Protozoa* protozoa, sf::Vector2f mousePosition, bool tolerance_check) const;
	const Cell* get_selected_cell(const Protozoa* protozoa, sf::Vector2f mouse_pos);

	inline static constexpr int max_evolutionary_iterations = 5;
	inline static constexpr int desired_cell_count = 4;


	Protozoa* find_protozoa_by_id(const int id)
	{
		return all_protozoa_.at(id);
	}


	void create_offspring(Protozoa* parent, const bool should_mutate = true)
	{
		Protozoa* offspring = get_unallocated_protozoa();
		offspring->create_offspring(parent, should_mutate);
	}

protected:
	static inline void generate_protozoa(Protozoa& protozoa, const Circle& world_bounds, bool emplace_back = true)
	{
		if (!protozoa.get_cells().empty())
		{
			std::cout << "[WARNING]: Protozoa already has cells, generation process may produce unexpected results.\n";
		}

		// we dont need to create protozoas for the "first time" during extinction or reset events
		protozoa.hard_reset();

		sf::Vector2f pos = world_bounds.rand_pos();
		protozoa.init_one_cell(pos);
		


		for (int i = 0; i < max_evolutionary_iterations; ++i)
		{
			protozoa.mutate(true, 0.8f, 0.43f);

			// as soon as we reach the criteria passing the desired cell count, the chance of breaking out the loop increases 
			// gradually until max evolutionary iterations
			const size_t cell_count = protozoa.get_cells().size();
			if (cell_count < desired_cell_count)
				continue;
			
			float break_chance = static_cast<float>(cell_count - desired_cell_count) / float(max_evolutionary_iterations - desired_cell_count);
			if (Random::rand01_float() < break_chance)
				break;
		}
	}

	void update_all_protozoa(FoodManager& food_manager_, const bool debug_mode, const float min_speed, const bool track_statistics, bool collisions)
	{

		std::vector<int> reproduce_indexes{};
		reproduce_indexes.reserve(max_protozoa);

		for (Protozoa* protozoa : all_protozoa_)
		{
			protozoa->update();

			if (!protozoa->is_alive())
			{
				if (track_statistics)
					register_death_stat(protozoa->frames_alive, protozoa->offspring_count > 0);
				all_protozoa_.remove(protozoa);
				continue;
			}

			if (protozoa->should_reproduce())
			{
				reproduce_indexes.push_back(protozoa->id);
			}
		}

		for (int idx : reproduce_indexes)
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
				cell.position_ += collision_resolutions[idx++];
			}
		}
#
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

	


	void check_for_extinction_event(Circle& world_bounds)
	{
		// if protozoas are still alivee or if auto reset on extinction is disabled, we dont need to do anything
		if (all_protozoa_.size() > 0 || !auto_reset_on_extinction)
			return;

		std::cout << "Extinction event occurred, respawning initial protozoa...\n";

		for (unsigned i = 0; i < initial_protozoa; ++i)
		{
			Protozoa* protozoa = all_protozoa_.add();
			generate_protozoa(*protozoa, world_bounds, false);
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
};