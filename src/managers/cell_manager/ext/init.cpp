#include "../cell_manager.h"

CellManager::CellManager(sf::RenderWindow* window, WorldBorder* world_bounds, o_vector<Body>* bodies) : m_window_(window), world_bounds_(world_bounds), bodies_(bodies), all_cells_(max_protozoa), all_springs_(max_protozoa)
{
	// Initialize the cell manager with the given window, world bounds, and body vector
	create_new_protozoa(CellManagerSettings::initial_protozoa, world_bounds);

	// Reserve space for birth and connection requests to avoid frequent reallocations
	birth_requests.reserve(100);
	connection_requests.reserve(100);

	std::cout << "[INFO]: CellManager initialized with protozoa: " << all_cells_.size() << "\n";
	if (window == nullptr)
	{
		std::cerr << "[ERROR]: CellManager initialized with null window pointer.\n";
	}
}

void CellManager::create_new_protozoa(int count, WorldBorder* spawn_area)
{
	// The cells we currently have act as seeds that allow us to build the protozoa
	for (int i = 0; i < count; i++)
	{
		sf::Vector2f spawn_pos = spawn_area->rand_pos();
		CellBodyPair pair = create_cell(spawn_pos, true);
		if (!pair.is_valid())
			break;

		// we want to limit the number of cells in a protozoa to avoid performance issues 
		int max_recursion_depth = Random::rand_range(1, 4);
		if (!build_protozoa_from_seed(pair.cell_ptr, 1))
			break;
	}
	
	std::cout << "Finished building protozoa's, total: " << all_cells_.size() << "\n";
}