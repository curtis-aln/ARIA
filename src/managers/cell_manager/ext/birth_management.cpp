#include "../cell_manager.h"

/* Reproductive System

[Contextual Systems]
- A spring connects Two Cells together, Two cells can only be connected with no more than One Spring
- Cells share energy with eachover through springs which allows their energys to reach pass the birth threshold roughly at the same time.
- Cells cannot reproduce for a period of time after being born So offspring Dont mess up the reproductive process
- Cells have a reproductive cooldown So Two reproductive processes dont occour at the same time

[Method]
1. A cell's energy passes the reproductive threshold.
	1.1. reproduce is set to true in this cell
2. Iterate Through all the springs, and check if their cells have reproduce set to true
3. for each cell (reproducing), check if the spring cells already has an offspring index
   - if one or the other cell does have an offspring
   3.1. Create an offspring Request for the cell that doesn't have one
   - if they both have offspring cells
   3.1. Do Nothing
   - if neither have offspring
   3.2. create two offspring Requests
4. Set Reproduce to false for the cells which have offspring
5. Check if there is already a connection request made Between the two children
	5.1. if there is a connection request, do nothing
	5.2. if there isnt a connection request, make one

6. The joining connections we made from the parents to children are weak so they will break naturally
*/

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
	CellBodyPair pair = create_cell();
	if (!pair.is_valid())
		return false;

	Body* seed_body = bodies_->at(seed_cell->body_id_);

	Body* child_body = get_cell_body(pair.cell_ptr->id_);
	child_body->position_ = Random::rand_position_in_circle(seed_body->position_, seed_body->radius_ * 3.5f);

	seed_cell = all_cells_.at(seed_cell->id_);           // re-resolve — seed_body isn't used again, so it's fine as-is

	Spring* spring = create_spring(seed_cell->id_, pair.cell_ptr->id_);
	if (spring == nullptr)
		return false;
	spring->randomize();

	if (recursion_depth < max_recursion_depth)
		build_protozoa_from_seed(pair.cell_ptr, max_recursion_depth, recursion_depth + 1);

	return true;
}


// ─────────────────────────────────────────────────────────────────────────────
//  collect_reproduction_requests
//
//  Scans cells for two kinds of pending reproductive events and queues them
//  for deferred processing (applied at end of update, not mid-iteration).
//
//  BIRTH REQUEST (cell.reproduce == true):
//    Cell has enough energy and wants an offspring. We clear the flag and
//    queue a BirthRequest. apply_birth_requests() handles it next.
//
//  CONNECTION REQUEST (cell.spring_to_copy_index >= 0):
//    Both this cell and its spring-partner now have valid offspring indexes,
//    meaning two new cells exist and need wiring together with a spring.
//    We queue a ConnectionRequest and reset all three reproductive fields.
//
// ─────────────────────────────────────────────────────────────────────────────
void CellManager::collect_reproduction_requests()
{
	constexpr uint16_t OFFSPRING_CONNECTION_TIMEOUT = 100; // frames

	for (Cell* cell : all_cells_)
	{
		if (cell->should_reproduce())
		{
			cell->turn_off_reproduction();
			birth_requests.push_back({ cell->id_ });
		}

		// Abandon an offspring link that's been waiting too long for its
		// sibling to arrive. Guard on spring_to_copy_index < 0 so we never
		// cancel a connection that resolved on THIS tick.
		if (cell->offspring_index >= 0
			&& cell->spring_to_copy_index < 0
			&& cell->frames_since_offspring_pending_ > OFFSPRING_CONNECTION_TIMEOUT)
		{
			cell->offspring_index = -1;
			cell->connection_index = -1;
			cell->spring_to_copy_index = -1;
			cell->frames_since_offspring_pending_ = 0;
			continue;
		}

		if (cell->spring_to_copy_index >= 0)
		{
			connection_requests.push_back(ConnectionRequest{
				cell->offspring_index,
				cell->connection_index,
				cell->spring_to_copy_index });
		}
	}
}


// ─────────────────────────────────────────────────────────────────────────────
//  apply_birth_requests
//
//  Processes all queued BirthRequests: spawns an offspring cell for each
//  parent and links them with a weak temporary spring to keep them close
//  while waiting for the permanent spring from apply_connection_requests().
//
//  The temporary spring is nearly slack (tiny spring_const, no oscillation,
//  heavy damping) — it just prevents the offspring from drifting out of
//  range before the real spring is created.
// ─────────────────────────────────────────────────────────────────────────────
void CellManager::apply_birth_requests()
{
	for (const BirthRequest& req : birth_requests)
	{
		CellBodyPair pair = create_cell();
		if (!pair.is_valid())
			break;

		// retrieve the parent cell
		Cell* parent_cell = all_cells_.at(req.parent_cell_id);
		Body* parent_body = bodies_->at(parent_cell->body_id_);

		Body* offspring_body = pair.body_ptr;
		Cell* offspring = pair.cell_ptr;
		
		// create the offspring by filling in its genetics and other properties based on the parent cell
		parent_cell->create_offspring(offspring, parent_body, offspring_body, true);
		parent_cell->turn_off_reproduction();

		// it's important to tell the parent cell which offspring is theirs, so we can apply connection requests
		parent_cell->offspring_index = offspring->id_;

		// when we create this offspring we create a spring to it, the spring has a weak connection as it is made to break
		// This is a temporary spring, it needs hold the new cell close to the parent cell until the real spring is made
		// this is because if the two new cells are too far apart when the spring is made, the spring will break immediately and the offspring will die before it can reproduce

		
		Spring* spring = all_springs_.emplace(true, true);
		spring->reset();

		spring->spring_const = 0.00005f;
		spring->amplitude = 0.4f;
		spring->damping = 0.0005;

		spring->cell_A_id = parent_cell->id_;
		spring->cell_B_id = offspring->id_;

		const sf::Vector2f diff = parent_body->position_ - offspring_body->position_;
		spring->rest_length = diff.length() * 3;
	}

	birth_requests.clear(); 
}



// ─────────────────────────────────────────────────────────────────────────────
//  apply_connection_requests
//
//  Creates the permanent spring between two offspring cells, completing the
//  reproductive cycle. Spring genetics are inherited (and optionally mutated)
//  from the spring that originally connected their parent cells.
//
//  Full reproductive cycle recap:
//    1. parent pair has enough energy → cell.reproduce = true
//    2. collect_reproduction_requests() queues BirthRequests
//    3. apply_birth_requests() spawns offspring + temporary spring
//    4. handle_reproduction() (in Spring) detects both offspring_indexes are set
//    5. collect_reproduction_requests() queues ConnectionRequest
//    6. apply_connection_requests() creates permanent spring → cycle complete
//
// ─────────────────────────────────────────────────────────────────────────────
void CellManager::apply_connection_requests()
{
	for (const ConnectionRequest& req : connection_requests)
	{
		Spring* new_spring = all_springs_.emplace(true, true);
		new_spring->reset();
		new_spring->cell_A_id = req.offspring_id;
		new_spring->cell_B_id = req.connect_to_id;

		Spring* parent_spring = all_springs_.at(req.spring_to_copy_index);
		parent_spring->create_offspring(*new_spring);
		
		Cell* cell = all_cells_.at(parent_spring->cell_A_id);
		Cell* other_cell = all_cells_.at(parent_spring->cell_B_id);

		// reset the reproductive fields so we don't create multiple connection requests for the same cells
		cell->connection_index = -1;
		cell->offspring_index = -1;
		cell->spring_to_copy_index = -1;

		other_cell->connection_index = -1;
		other_cell->offspring_index = -1;
		other_cell->spring_to_copy_index = -1;
	}

	connection_requests.clear();
}