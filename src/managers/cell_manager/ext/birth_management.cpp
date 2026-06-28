#include "../cell_manager.h"

// ─────────────────────────────────────────────────────────────────────────────
//  build_protozoa_from_seed
//
//  Recursively builds a chain of cells connected by springs, starting from
//  a single seed cell. Used at simulation start and after extinction events.
//
//  Each call spawns one child cell near the seed, creates a spring between
//  them, then recurses into the child until max_recursion_depth is reached.
//  The result is a linear chain: seed → child → grandchild → ...
//
//  Returns false if body allocation fails (pool exhausted), true otherwise.
// ─────────────────────────────────────────────────────────────────────────────
bool CellManager::build_protozoa_from_seed(Cell* seed_cell, int max_recursion_depth, int recursion_depth)
{
	// This function takes in a seed cell and builds a protozoa around it, with a random number of cells and springs
	// This happens only at the start of the simulation or whenver we want to reset it (e.g. extinction event)
	// It is a recursive function meaning it calls itself until a certain depth is reached

	// The body of the seed cell needs to be retrieved so we can spawn the new cells around it
	Body* seed_body = bodies_->at(seed_cell->body_id_);

	// The First child is made next to the seed_cell, within close proximity so that the spring can be made without breaking immediately
	float spawn_dist = seed_body->radius_ * 3.5f;
	sf::Vector2f child_pos = Random::rand_position_in_circle(seed_body->position_, spawn_dist);

	// we have all the information we need to spawn the next cell
	Cell* child_cell = all_cells_.emplace(true, false);

	if (!link_cell_to_body(child_cell, true, child_pos))
	{
		all_cells_.remove(child_cell); // dont forget to remove the cell if we fail to link it to a body
		return false;
	}
	Body* child_body = bodies_->at(child_cell->body_id_);
	child_body->position_ = child_pos;

	// now that the child cell is spawned, we can create a spring between the seed cell and the child cell
	Spring* spring = all_springs_.emplace(true);
	spring->reset();
	spring->cell_A_id = seed_cell->id_;
	spring->cell_B_id = child_cell->id_;

	if (recursion_depth < max_recursion_depth)
	{
		build_protozoa_from_seed(child_cell, max_recursion_depth, recursion_depth + 1);
	}

	return true;
}


void CellManager::collect_reproduction_requests()
{

	for (Cell* cell : all_cells_)

		if (cell->reproduce)
		{
			cell->reproduce = false;
			birth_requests.push_back({ cell->id_ });

			if (Random::rand01_float() < 0.01)
				break;
		}
		else if (cell->spring_to_copy_index >= 0)
		{
			connection_requests.push_back(ConnectionRequest{
				cell->offspring_index,
				cell->connection_index,
				cell->spring_to_copy_index
				});
			cell->connection_index = -1;
			cell->offspring_index = -1;
			cell->spring_to_copy_index = -1;

			if (Random::rand01_float() < 0.01)
				break;
		}
	}


void CellManager::apply_birth_requests()
{

	for (const BirthRequest& req : birth_requests)
	{
		if (bodies_->can_add() == false)
			break;

		Cell* offspring = all_cells_.emplace(true, true);

		Body* offspring_body = bodies_->add();

		Cell* parent_cell = all_cells_.at(req.parent_cell_id);
		parent_cell->create_offspring(offspring, offspring_body);
		offspring->body_id_ = offspring_body->id_;

		// it's important to tell the parent cell which offspring is theirs, so we can apply connection requests
		parent_cell->offspring_index = offspring->id_;

		// when we create this offspring we create a spring to it, the spring has a weak connection as it is made to break
		// This is a temporary spring, it needs hold the new cell close to the parent cell until the real spring is made
		// this is because if the two new cells are too far apart when the spring is made, the spring will break immediately and the offspring will die before it can reproduce

		Spring* spring = all_springs_.emplace(true);

		spring->spring_const = 0.00001f;
		spring->amplitude = 0.f;
		spring->damping = 0.9f;

		spring->cell_A_id = parent_cell->id_;
		spring->cell_B_id = offspring->id_;

		Body* parent_body = bodies_->at(parent_cell->body_id_);
		const sf::Vector2f diff = parent_body->position_ - offspring_body->position_;
		spring->rest_length = diff.length();
	}

	birth_requests.clear(); 
}

void CellManager::apply_connection_requests()
{
	for (const ConnectionRequest& req : connection_requests)
	{
		// we need to check if the spring to copy index is valid, if it is we copy the spring data to the new spring
		const bool valid_spring_to_copy = req.spring_to_copy_index >= 0
			&& req.spring_to_copy_index < all_springs_.size();

		Spring* new_spring = all_springs_.emplace(true, true);
		new_spring->cell_A_id = req.offspring_id;
		new_spring->cell_B_id = req.connect_to_id;

		if (valid_spring_to_copy)
		{
			Spring* parent_spring = all_springs_.at(req.spring_to_copy_index);
			parent_spring->create_offspring(*new_spring);
		}

	}

	connection_requests.clear();
}