#include "../cell_manager.h"

CellManager::CellManager(sf::RenderWindow* window, WorldBorder* world_bounds) : m_window_(window), world_bounds_(world_bounds), all_cells_(max_protozoa), all_springs_(max_protozoa)
{
	birth_requests.reserve(10);
	connection_requests.reserve(10);

	std::cout << "[INFO]: CellManager initialized with max protozoa: " << max_protozoa << "\n";
	if (window == nullptr)
	{
		std::cerr << "[ERROR]: CellManager initialized with null window pointer.\n";
	}
}

void CellManager::init_protozoa_container()
{
	const int max_cells = max_protozoa; // 
	// We create the maximum amount of protozoa at the start
	for (int i = 0; i < max_cells; ++i)
	{
		all_cells_.emplace(i);
		all_springs_.emplace(i);
	}

	// removing any protozoa that are above the initial protozoa count
	for (int i = 0; i < max_cells; ++i)
	{
		all_cells_.remove(i);
	}

	const int size = all_springs_.size();
	for (int i = 0; i < size; ++i)
	{
		all_springs_.remove(i);
	}

	for (int i = 0; i < initial_protozoa; ++i)
	{
		build_protozoa();
	}
}