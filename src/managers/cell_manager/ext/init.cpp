#include "../cell_manager.h"

CellManager::CellManager(sf::RenderWindow* window, WorldBorder* world_bounds, o_vector<Body>* bodies) : m_window_(window), world_bounds_(world_bounds), bodies_(bodies), all_cells_(max_protozoa), all_springs_(max_protozoa)
{
	// Initialize the cell manager with the given window, world bounds, and body vector
	create_new_protozoa(CellManagerSettings::initial_protozoa, world_bounds);

	// Reserve space for birth and connection requests to avoid frequent reallocations
	birth_requests.reserve(10);
	connection_requests.reserve(10);

	std::cout << "[INFO]: CellManager initialized with max protozoa: " << max_protozoa << "\n";
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
		Cell* cell = all_cells_.emplace(true, true);

		if (!link_cell_to_body(cell, true, spawn_area->rand_pos()))
		{
			std::cerr << "[ERROR]: Failed to link cell to body during initialization. Max bodies reached.\n";
			break;
		}

		int max_recursion_depth = Random::rand_range(1, 4); // we want to limit the number of cells in a protozoa to avoid performance issues
		if (!build_protozoa_from_seed(cell, 1))
		{
			break;
		}
	}
	
	std::cout << "Finished building protozoa's, total: " << all_cells_.size() << "\n";
}

bool CellManager::link_cell_to_body(Cell* cell, bool is_active, sf::Vector2f pos)
{
	// This function creates a new body for the cell and links them together
	// returns true if successful, false if there are no more bodies available
	Body* body = bodies_->emplace(is_active);
	if (body == nullptr)
	{
		return false;
	}
	
	body->position_ = pos;
	body->radius_ = cell->radius;

	cell->body_id_ = body->id_;
	return true;
}