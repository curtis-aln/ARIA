#include "../cell_manager.h"

CellManager::CellManager(sf::RenderWindow* window, WorldBorder* world_bounds, o_vector<Body>* bodies) : m_window_(window), world_bounds_(world_bounds), bodies_(bodies), all_cells_(max_protozoa), all_springs_(max_protozoa)
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
	// The cells container needs to be in sync with the bodies container

	// We create the maximum amount of protozoa at the start
	for (int i = 0; i < max_protozoa; ++i)
	{
		all_cells_.emplace(i);
		all_springs_.emplace(i);
	}

	// removing any protozoa that are above the initial protozoa count
	for (int i = 0; i < max_protozoa; ++i)
	{
		all_cells_.remove(i);
		all_springs_.emplace(i);
	}

	for (int i = 0; i < initial_protozoa; ++i)
	{
		if (!build_protozoa())
		{
			break;
		}
	}

	std::cout << "Finished building protozoa's, total: " << all_cells_.size() << "\n";
}