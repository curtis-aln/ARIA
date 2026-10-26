#include "../cell_manager.h"

float CellManager::calculate_average_generation() const
{
	// when a cell reproduces it sets its offspring generation to 1 + its current, this can be used to track 
	// how many generations have passed in the simulation, and can be used to measure the evolutionary progress of the protozoa
	if (all_cells_.size() == 0) // extinction
		return 0.f;

	float sum = 0.f;
	int count = 0;

	for (Cell* cell : all_cells_)
	{
		sum += cell->get_generation();
		count++;
	}

	return sum / count;
}

void CellManager::register_birth_stat()
{
	// This function is called whenever a new cell is born, it increments the births_this_window_ counter
	statistics_.births_this_window++;
}

void CellManager::register_death_stat(const float lifetime, const bool had_offspring)
{
	recent_lifetimes_.push_back(lifetime);
	recent_lifetimes_sum_ += lifetime;

	if (recent_lifetimes_.size() > max_lifetime_samples_)
	{
		recent_lifetimes_sum_ -= recent_lifetimes_.front();
		recent_lifetimes_.pop_front();
	}

	statistics_.average_lifetime = recent_lifetimes_.empty()
		? 0.f
		: recent_lifetimes_sum_ / static_cast<float>(recent_lifetimes_.size());

	statistics_.deaths_this_window++;
	++statistics_.total_deaths;

	if (!had_offspring)
	{
		++statistics_.non_repro_deaths_this_window;
	}
	if (lifetime < 30.f)
	{
		++statistics_.infant_deaths_this_window;
	}

	statistics_.longest_lived_ever = std::max(static_cast<uint16_t>(lifetime), statistics_.longest_lived_ever);
}

const std::vector<float>& CellManager::get_generation_distribution()
{
	distribution_.clear();

	for (const Cell* cell : all_cells_)
		distribution_.push_back(static_cast<float>(cell->get_generation()));

	return distribution_;
}

void CellManager::update_100frame_stats(int iterations)
{
	if (iterations % survival_rate_window_size_ == 0)
	{
		statistics_.deaths_per_hundered_frames = static_cast<float>(statistics_.deaths_this_window);
		statistics_.births_per_hundered_frames = static_cast<float>(statistics_.births_this_window);
		statistics_.non_repro_deaths_per_hundered_frames = static_cast<float>(statistics_.non_repro_deaths_this_window);

		statistics_.infant_mortality_rate = statistics_.deaths_this_window > 0
			? static_cast<float>(statistics_.infant_deaths_this_window) / static_cast<float>(statistics_.deaths_this_window)
			: 0.f;

		// Reset window
		statistics_.deaths_this_window = 0;
		statistics_.births_this_window = 0;
		statistics_.infant_deaths_this_window = 0;
		statistics_.non_repro_deaths_this_window = 0;
	}
}


void CellManager::update_protozoa_tracker()
{
	protozoa_tracker_.update_primitive(selected_cell_id_, all_cells_, all_springs_, *bodies_);
}

void CellManager::update_statistics()
{
	statistics_.min_speed += delta_min_speed;

	// Selected cell Logic
	statistics_.average_generation = calculate_average_generation();


	// Peak population
	const int count = static_cast<int>(all_cells_.size());
	statistics_.peak_protozoa_ever = std::max(count, statistics_.peak_protozoa_ever);
	statistics_.cell_count = count;
	statistics_.spring_count = static_cast<int>(all_springs_.size());
	statistics_.cell_matter_count = static_cast<int>(all_cell_matter_.size());

	// resetting averages
	statistics_.average_offspring_count = 0.f;
	statistics_.average_mutation_rate = 0.f;
	statistics_.average_mutation_range = 0.f;
	statistics_.average_energy = 0.f;
	statistics_.average_generation = 0.f;

	// collecting data
	for (Cell* cell : all_cells_)
	{
		statistics_.average_mutation_rate += cell->guassian_const;
		statistics_.average_mutation_range += cell->mutation_range;
		statistics_.average_offspring_count += cell->offspring_count;
		statistics_.average_energy += cell->get_energy();
		statistics_.average_generation += cell->get_generation();

		statistics_.highest_generation_ever = std::max(cell->get_generation(), statistics_.highest_generation_ever);
		statistics_.most_offspring_ever = std::max(static_cast<int>(cell->offspring_count), statistics_.most_offspring_ever);
	}

	// calculating averages
	statistics_.average_offspring_count /= count;
	statistics_.average_mutation_rate /= count;
	statistics_.average_mutation_range /= count;
	statistics_.average_energy /= count;
	statistics_.average_generation /= count;



	statistics_.spring_breaking_force = Spring::spring_break_force;
	statistics_.spring_breaking_length = Spring::spring_break_length_factor;
	statistics_.spring_damage_threshold = Spring::spring_damage_threshold;
	statistics_.spring_work_const = Spring::spring_work_const;
}