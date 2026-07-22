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

	Spring::SPRING_BREAK_LENGTH = SpringSettings::breaking_length;
	Spring::SPRING_BREAK_FORCE = SpringSettings::spring_break_force;
	Spring::SPRING_DAMAGE_THRESH = SpringSettings::spring_damage_threshold;
	Spring::SPRING_WORK_CONST = SpringSettings::spring_work_const;
}


void CellManager::ensure_update_jobs_built()
{
	if (update_jobs_built_)
		return;

	int updating_threads = WorldSettings::updating_threads;

	updating_bodies_.clear();
	updating_bodies_.reserve(updating_threads);

	// For each of the threads
	for (int t = 0; t < (int)updating_threads; ++t)
	{
		updating_bodies_.emplace_back([this, t, updating_threads] {
			const int total_cells = current_total_cells_;
			if (total_cells == 0)
				return;

			const int chunk = std::max(1, (total_cells + (int)updating_threads - 1) / (int)updating_threads);
			const int begin = t * chunk;
			if (begin >= total_cells)
				return;
			const int end = std::min(begin + chunk, total_cells);

			for (int k = begin; k < end; ++k)
			{
				Cell* cell = all_cells_.at(all_cells_.occupied_list[k]);
				update_cell(cell);

			}});
	}

	thread_pool_.set_jobs(updating_bodies_);   // only ever called this once
	update_jobs_built_ = true;
}


void CellManager::reset()
{
	birth_requests.clear();
	connection_requests.clear();
	recent_lifetimes_.clear();
	distribution_.clear();
	selected_cell = nullptr;
	
	for (Cell* cell : all_cells_)
	{
		all_cells_.remove(cell->id_);
		bodies_->remove(cell->body_id_);
		cell->recreate();
	}

	for (Spring* spring : all_springs_)
	{
		all_springs_.remove(spring->id_);
	}

	create_new_protozoa(CellManagerSettings::initial_protozoa, world_bounds_);
}

bool CellManager::has_cell_with_body_id(int body_id)
{
	for (Cell* cell : all_cells_)
	{
		if (cell->body_id_ == body_id)
			return true;
	}
	return false;
}

void CellManager::create_new_protozoa(int count, WorldBorder* spawn_area)
{
	// The cells we currently have act as seeds that allow us to build the protozoa
	for (int i = 0; i < count; i++)
	{
		sf::Vector2f spawn_pos = spawn_area->rand_pos();
		CellBodyPair pair = create_cell(spawn_pos, true);

		if (!pair.is_valid)
			break;

		// we want to limit the number of cells in a protozoa to avoid performance issues 
		int max_recursion_depth = Random::rand_range(1, 2);
		if (!build_protozoa_from_seed(pair.cell_id, max_recursion_depth))
			break;
	}
	
}