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
	bool is_object_active = true; // the first few objects will be active, the rest will be inactive
	for (int i = 0; i < initial_protozoa; ++i)
	{
		Cell* cell = all_cells_.emplace(is_object_active);

		if (!link_cell_to_body(cell, is_object_active))
		{
			std::cerr << "[ERROR]: Failed to link cell to body during initialization. Max bodies reached.\n";
			break;
		}

		if (i >= initial_protozoa)
		{
			is_object_active = false; // the rest of the objects will be inactive
		}
	}

	// The cells we currently have act as seeds that allow us to build the protozoa
	for (Cell* cell : all_cells_)
	{
		if (!build_protozoa_from_seed(cell))
		{
			break;
		}
	}
	
	std::cout << "Finished building protozoa's, total: " << all_cells_.size() << "\n";
}

bool CellManager::link_cell_to_body(Cell* cell, bool is_active)
{
	// This function creates a new body for the cell and links them together
	// returns true if successful, false if there are no more bodies available
	Body* body = bodies_->emplace(is_active);
	if (body == nullptr)
	{
		return false;
	}
	
	body->position_ = world_bounds_->rand_pos();
	body->radius_ = cell->radius;

	cell->body_id_ = body->id_;
	return true;
}