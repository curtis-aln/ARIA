#include "../cell_manager.h"

bool CellManager::build_protozoa_from_seed(Cell* seed_cell)
{
	// This function takes in a seed cell and builds a protozoa around it, with a random number of cells and springs
	// This happens only at the start of the simulation or whenver we want to reset it (e.g. extinction event)

	// The body of the seed cell needs to be retrieved so we can spawn the new cells around it
	Body* seed_body = bodies_->at(seed_cell->body_id_);

	// The First child is made next to the seed_cell, within close proximity so that the spring can be made without breaking immediately
	float spawn_dist = seed_body->radius_ * 3.5f;
	sf::Vector2f child_pos = Random::rand_position_in_circle(seed_body->position_, spawn_dist);

	// we have all the information we need to spawn the next cell
	Cell* child_cell = all_cells_.emplace(true);

	if (!link_cell_to_body(child_cell, true))
	{
		all_cells_.remove(child_cell); // dont forget to remove the cell if we fail to link it to a body
		return false;
	}
	Body* child_body = bodies_->at(child_cell->body_id_);
	child_body->position_ = child_pos;

	// now that the child cell is spawned, we can create a spring between the seed cell and the child cell
	Spring* spring = all_springs_.emplace(true);
	spring->cell_A_id = seed_cell->id_;
	spring->cell_B_id = child_cell->id_;

	return true;

	/*
	// Now we create springs between the cells
	for (int i = 0; i < spring_count; ++i)
	{
		if (cells.size() < 2)
			return true;

		// choose two different cell ids
		int cell_A_id = Random::rand_range(size_t(0), cells.size() - 1);
		int cell_B_id = Random::rand_range(size_t(0), cells.size() - 1);

		if (cell_A_id == cell_B_id)
			return true;

		// check if a spring already exists between these two cells, if so we don't add another one
		for (const Spring* spring : springs)
		{
			if ((spring->cell_A_id == cell_A_id && spring->cell_B_id == cell_B_id) ||
				(spring->cell_A_id == cell_B_id && spring->cell_B_id == cell_A_id))
			{
				return true; // spring already exists
			}
		}

		Spring* spring = all_springs_.add();
		if (spring == nullptr)
		{
			return false;
		}
		spring->cell_A_id = cell_A_id;
		spring->cell_B_id = cell_B_id;
	}

	std::cout << "built\n";
	*/
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
			birth_requests.push_back({ cell->body_id_ });

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
	return; // todo
	cells.reserve(cells.size() + birth_requests.size());
	springs.reserve(springs.size() + birth_requests.size());

	for (const BirthRequest& req : birth_requests)
	{
		cells.emplace_back();
		Cell* offspring = &cells.back();

		if (bodies_->can_add() == false)
			return;

		Body* body = bodies_->add();

		cells[req.parent_cell_id].create_offspring(offspring, body);
		offspring->body_id_ = body->id_;

		// it's important to tell the parent cell which offspring is theirs, so we can apply connection requests
		cells[req.parent_cell_id].offspring_index = offspring->body_id_;

		// when we create this offspring we create a spring to it, the spring has a weak connection as it is made to break
		// This is a temporary spring, it needs hold the new cell close to the parent cell until the real spring is made
		// this is because if the two new cells are too far apart when the spring is made, the spring will break immediately and the offspring will die before it can reproduce

		const uint8_t spring_id = static_cast<uint8_t>(springs.size());
		springs.emplace_back(spring_id, req.parent_cell_id, offspring->body_id_);
		springs.back().spring_const = 0.00001f;
		springs.back().amplitude = 0.f;
		springs.back().damping = 0.9f;

		Body* parent_body = bodies_->at(req.parent_cell_id);
		const sf::Vector2f diff = parent_body->position_ - body->position_;
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