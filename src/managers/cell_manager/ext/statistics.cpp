#include "../cell_manager.h"

int CellManager::get_cell_count() const
{
	// the amount of cells alive in the simulation
	return all_cells_.size();
}

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
		sum += cell->generation;
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
	// This function is called whenever a cell dies, it updates the statistics related to cell deaths, 
	// including average lifetime and infant mortality rate.
	recent_lifetimes_.push_back(lifetime);
	if (recent_lifetimes_.size() > max_lifetime_samples_)
		recent_lifetimes_.erase(recent_lifetimes_.begin());

	float sum = 0.f;
	for (float l : recent_lifetimes_) sum += l;
	statistics_.average_lifetime = recent_lifetimes_.empty() ? 0.f : sum / recent_lifetimes_.size();

	statistics_.deaths_this_window++;
	++statistics_.total_deaths;
	if (!had_offspring) ++statistics_.infant_deaths;
	statistics_.infant_mortality_rate = statistics_.total_deaths > 0
		? static_cast<float>(statistics_.infant_deaths) / statistics_.total_deaths : 0.f;

	statistics_.longest_lived_ever = std::max(static_cast<uint16_t>(lifetime), statistics_.longest_lived_ever);
}

const std::vector<float>& CellManager::get_generation_distribution()
{
	distribution_.clear();

	for (const Cell* cell : all_cells_)
		distribution_.push_back(static_cast<float>(cell->generation));

	return distribution_;
}


void CellManager::update_statistics()
{
	// Selected cell Logic
	statistics_.selected_a_cell = selected_cell != nullptr;
	if (selected_cell != nullptr)
	{
		protozoa_tracker_.update_primitive(selected_cell->id_, all_cells_, all_springs_, *bodies_);
	}

	statistics_.average_generation = calculate_average_generation();

	// Updating death and birth rates per hundred frames TODO
	//if (iterations % survival_rate_window_size_ == 0)
	//{
	//	statistics_.deaths_per_hundered_frames = static_cast<float>(deaths_this_window_);
	//	statistics_.births_per_hundered_frames = static_cast<float>(births_this_window_);

	//	// Reset window
	//	deaths_this_window_ = 0;
	//	births_this_window_ = 0;
	//}

	// Peak population
	const int p_count = static_cast<int>(all_cells_.size());
	statistics_.peak_protozoa_ever = std::max(p_count, statistics_.peak_protozoa_ever);

	const int count = statistics_.cell_count;

	// resetting averages
	statistics_.average_offspring_count = 0.f;
	statistics_.average_mutation_rate = 0.f;
	statistics_.average_mutation_range = 0.f;
	statistics_.average_energy = 0.f;
	statistics_.average_generation = 0.f;

	// collecting data
	for (Cell* cell : all_cells_)
	{
		statistics_.average_mutation_rate += cell->mutation_rate;
		statistics_.average_mutation_range += cell->mutation_range;
		statistics_.average_offspring_count += cell->offspring_count;
		statistics_.average_energy += cell->energy;
		statistics_.average_generation += cell->generation;

		statistics_.highest_generation_ever = std::max(cell->generation, statistics_.highest_generation_ever);
		statistics_.most_offspring_ever = std::max(static_cast<int>(cell->offspring_count), statistics_.most_offspring_ever);
	}

	// calculating averages
	statistics_.average_offspring_count /= count;
	statistics_.average_mutation_rate /= count;
	statistics_.average_mutation_range /= count;
	statistics_.average_energy /= count;
	statistics_.average_generation /= count;


	statistics_.spring_breaking_force = Spring::SPRING_BREAK_FORCE;
	statistics_.spring_breaking_length = Spring::SPRING_BREAK_LENGTH;
	statistics_.spring_damage_threshold = Spring::SPRING_DAMAGE_THRESH;
	statistics_.spring_work_const = Spring::SPRING_WORK_CONST;
}