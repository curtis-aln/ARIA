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
	births_this_window_++;
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
	average_lifetime_ = recent_lifetimes_.empty() ? 0.f : sum / recent_lifetimes_.size();

	deaths_this_window_++;
	++total_deaths_;
	if (!had_offspring) ++infant_deaths_;
	infant_mortality_rate_ = total_deaths_ > 0
		? static_cast<float>(infant_deaths_) / total_deaths_ : 0.f;

	longest_lived_ever_ = std::max(static_cast<uint16_t>(lifetime), longest_lived_ever_);
}

const std::vector<float>& CellManager::get_generation_distribution()
{
	distribution_.clear();

	for (const Cell* cell : all_cells_)
		distribution_.push_back(static_cast<float>(cell->generation));

	return distribution_;
}

void CellManager::fill_statistics(WorldStatistics& stats)
{
	stats.average_generation = calculate_average_generation();

	stats.longest_lived_ever = longest_lived_ever_;
	stats.most_offspring_ever = most_offspring_ever_;
	stats.infant_mortality_rate = infant_mortality_rate_;
	stats.average_lifetime = average_lifetime_;

	stats.total_deaths = total_deaths_;
	stats.infant_deaths = infant_deaths_;
	stats.deaths_this_window = deaths_this_window_;
	stats.births_this_window = births_this_window_;

	// Updating death and birth rates per hundred frames
	if (stats.iterations_ % survival_rate_window_size_ == 0)
	{
		stats.deaths_per_hundered_frames = static_cast<float>(deaths_this_window_);
		stats.births_per_hundered_frames = static_cast<float>(births_this_window_);

		// Reset window
		deaths_this_window_ = 0;
		births_this_window_ = 0;
	}

	// Peak population
	const int p_count = static_cast<int>(all_cells_.size());
	stats.peak_protozoa_ever = std::max(p_count, stats.peak_protozoa_ever);

	const int count = stats.cell_count;

	// resetting averages
	stats.average_offspring_count = 0.f;
	stats.average_mutation_rate = 0.f;
	stats.average_mutation_range = 0.f;
	stats.average_energy = 0.f;
	stats.average_generation = 0.f;

	// collecting data
	for (Cell* cell : all_cells_)
	{
		stats.average_mutation_rate += cell->mutation_rate;
		stats.average_mutation_range += cell->mutation_range;
		stats.average_offspring_count += cell->offspring_count;
		stats.average_energy += cell->energy;
		stats.average_generation += cell->generation;

		stats.highest_generation_ever = std::max(cell->generation, stats.highest_generation_ever);
		stats.most_offspring_ever = std::max(static_cast<int>(cell->offspring_count), stats.most_offspring_ever);
	}

	// calculating averages
	stats.average_offspring_count /= count;
	stats.average_mutation_rate /= count;
	stats.average_mutation_range /= count;
	stats.average_energy /= count;
	stats.average_generation /= count;
}