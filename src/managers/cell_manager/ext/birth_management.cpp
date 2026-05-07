#include "../cell_manager.h"

void CellManager::build_protozoa()
{
	sf::Vector2f spawn_position = world_bounds_->rand_pos();
	float spawn_dist = 20.f;
	sf::FloatRect spawn_rect = { spawn_position - sf::Vector2f(spawn_dist, spawn_dist), sf::Vector2f(spawn_dist * 2, spawn_dist * 2) };
	const int cell_count = Random::rand_range(2, desired_cell_count);
	const int spring_count = Random::rand_range(cell_count - 1, cell_count * 2);

	std::vector<Cell*> cells; // a temporary cell vector
	std::vector<Spring*> springs; // a temporary spring vector

	for (int i = 0; i < cell_count; ++i)
	{
		const sf::Vector2f cell_pos = Random::rand_pos_in_rect(spawn_rect);

		Cell* cell = all_cells_.add();
		cell->reset();
		cell->set_pos(cell_pos);
		cells.push_back(cell);
	}

	// Now we create springs between the cells
	for (int i = 0; i < spring_count; ++i)
	{
		if (cells.size() < 2)
			return;

		// choose two different cell ids
		int cell_A_id = Random::rand_range(size_t(0), cells.size() - 1);
		int cell_B_id = Random::rand_range(size_t(0), cells.size() - 1);

		if (cell_A_id == cell_B_id)
			return; // better luck next time

		// check if a spring already exists between these two cells, if so we don't add another one
		for (const Spring* spring : springs)
		{
			if ((spring->cell_A_id == cell_A_id && spring->cell_B_id == cell_B_id) ||
				(spring->cell_A_id == cell_B_id && spring->cell_B_id == cell_A_id))
			{
				return; // spring already exists
			}
		}

		Spring* spring = all_springs_.add();
		if (spring == nullptr)
		{
			return;
		}
		spring->cell_A_id = cell_A_id;
		spring->cell_B_id = cell_B_id;
	}

	std::cout << "built\n";
}


void CellManager::collect_reproduction_requests(std::vector<Cell*>& cells)
{
	const int cell_count = static_cast<int>(cells.size());

	for (int i = 0; i < cell_count; ++i)
	{
		Cell* cell = cells[i];

		if (cell->reproduce)
		{
			cell->reproduce = false;
			birth_requests.push_back({ cell->id_ });

			if (Random::rand01_float() > 0.01)
				break;
		}
		else if (cell->spring_to_copy_index >= 0)
		{
			connection_requests.push_back({
				static_cast<uint8_t>(cell->offspring_index),
				static_cast<uint8_t>(cell->connection_index),
				cell->spring_to_copy_index
				});
			cell->connection_index = -1;
			cell->offspring_index = -1;
			cell->spring_to_copy_index = -1;

			if (Random::rand01_float() > 0.01)
				break;
		}
	}
}

void CellManager::apply_birth_requests(std::vector<Cell>& cells, std::vector<Spring>& springs)
{
	cells.reserve(cells.size() + birth_requests.size());
	springs.reserve(springs.size() + birth_requests.size());

	for (const BirthRequest& req : birth_requests)
	{
		const uint8_t offspring_id = static_cast<uint8_t>(cells.size());

		cells.emplace_back();
		Cell* offspring = &cells.back();
		cells[req.parent_cell_id].create_offspring(offspring);
		offspring->id_ = offspring_id;

		// it's important to tell the parent cell which offspring is theirs, so we can apply connection requests
		cells[req.parent_cell_id].offspring_index = offspring_id;

		// when we create this offspring we create a spring to it, the spring has a weak connection as it is made to break
		// This is a temporary spring, it needs hold the new cell close to the parent cell until the real spring is made
		// this is because if the two new cells are too far apart when the spring is made, the spring will break immediately and the offspring will die before it can reproduce

		const uint8_t spring_id = static_cast<uint8_t>(springs.size());
		springs.emplace_back(spring_id, req.parent_cell_id, offspring_id);
		springs.back().spring_const = 0.00001f;
		springs.back().amplitude = 0.f;
		springs.back().damping = 0.9f;

		const sf::Vector2f diff = cells[req.parent_cell_id].get_pos() - offspring->get_pos();
		springs.back().rest_length = diff.length();
	}
}

void CellManager::apply_connection_requests(std::vector<Cell>& cells, std::vector<Spring>& springs)
{
	springs.reserve(springs.size() + connection_requests.size());

	for (const ConnectionRequest& req : connection_requests)
	{
		// we need to check if the spring to copy index is valid, if it is we copy the spring data to the new spring
		const bool valid_spring_to_copy = req.spring_to_copy_index >= 0
			&& req.spring_to_copy_index < static_cast<int8_t>(springs.size());

		const uint8_t spring_id = static_cast<uint8_t>(springs.size());
		springs.emplace_back(spring_id, req.offspring_id, req.connect_to_id);
		Spring* new_spring = &springs.back();

		if (valid_spring_to_copy)
		{
			springs[req.spring_to_copy_index].create_offspring(*new_spring);
		}

	}
}