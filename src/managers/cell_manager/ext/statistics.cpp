#include "../cell_manager.h"

int CellManager::get_protozoa_count() const
{
	return 1; // todo
	//return all_protozoa_.size();
}

float CellManager::calculate_average_generation() const
{
	if (all_cells_.size() == 0)
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
	births_this_window_++;
}

void CellManager::register_death_stat(const float lifetime, const bool had_offspring)
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