#include "../cell_manager.h"
#include <cstdint>
#include <entities/body.h>
#include <entities/cell/cell.h>
#include <entities/matter/CellMatter.h>
#include <entities/spring/spring.h>
#include <iostream>
#include <SFML/System/Vector2.hpp>
#include <Utils/random.h>
#include <Utils/spatial_grid/simple_spatial_grid.h>
#include <vector>


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
	for (Cell* cell : all_cells_)
	{
		if (cell->should_reproduce())
		{
			cell_birth_requests.push_back({ cell->id_ });
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
void CellManager::apply_reproduction_requests()
{
	for (const BirthRequest& req : cell_birth_requests)
	{
		if (toggles_.easy_reproduction)   // new ImGui toggle, add it to CellManagerToggles
		{
			if (!all_cells_.is_obj_active(req.parent_cell_id))
				continue;

			if (can_protozoa_reproduce(req.parent_cell_id, protozoa_cell_ids_))
				protozoa_reproduce(protozoa_cell_ids_);

			all_cells_.at(req.parent_cell_id)->turn_off_reproduction();

			continue; // skip the natural path, including create_temporary_spring_connection
		}


		CellBodyPair pair = create_cell();
		if (!pair.is_valid)
			break;

		// retrieve the parent cell and body
		Cell* parent_cell = all_cells_.at(req.parent_cell_id);
		Body* parent_body = bodies_->at(parent_cell->body_id_);

		parent_cell->turn_off_reproduction();

		// retrieve the offspring cell and body
		Body* offspring_body = bodies_->at(pair.body_id);
		Cell* offspring_cell = all_cells_.at(pair.cell_id);

		// create the offspring by filling in its genetics and other properties based on the parent cell
		parent_cell->create_offspring(parent_body, offspring_cell, offspring_body, true);

		// connecting the parent and childS
		create_temporary_spring_connection(parent_cell->id_, offspring_cell->id_);

		// small random chance that this offspring has its own cell addition
		if (Random::rand01_float() < offspring_cell->add_cell_chance)
		{
			create_weak_offspring(offspring_cell->id_);
		}
	}

	cell_birth_requests.clear();
}

void CellManager::create_temporary_spring_connection(const cell_idx parent_id, const cell_idx offspring_id)
{
	// A weak breakable connection between the parent and child is made to keep the child around
	// for the full reproductive process
	int32_t new_spring_id = create_spring(parent_id, offspring_id);
	if (new_spring_id == -1)
		return;
	Spring* spring = all_springs_.at(new_spring_id);

	// Setting the spring attributes - it should keep a near constant length with no oscillation
	constexpr float spring_death_chance = 1.f / 180.f; // 1 in 300 chance of breaking per frame
	constexpr float temporary_spring_const = 0.1f;
	constexpr float  temporary_spring_damping = 0.5f;
	constexpr float temporary_spring_nutrient_rate = 0.f;

	spring->death_chance_ = spring_death_chance;

	spring->genome.frequency = 0.f;      // no oscillation
	spring->genome.amplitude = 0.f;      // no oscillation -> vertical_shift alone sets rest_length
	spring->genome.vertical_shift = 0.2f;
	spring->genome.spring_const = temporary_spring_const;
	spring->genome.damping = temporary_spring_damping;

	spring->genome.nutrient_transfer_rate = temporary_spring_nutrient_rate;
}


void CellManager::apply_connection_requests()
{
	for (const ConnectionRequest& req : connection_requests)
	{
		// first we need to check that both cells are still alive
		Cell* cell = all_cells_.at(req.offspring_id);
		Cell* other_cell = all_cells_.at(req.connect_to_id);

		if (all_cells_.is_obj_active(cell->id_) == false || all_cells_.is_obj_active(other_cell->id_) == false)
			continue;

		// if the distance is suspiciously large, print

		int32_t new_spring_id = create_spring(static_cast<uint32_t>(req.offspring_id), static_cast<uint32_t>(req.connect_to_id));

		if (new_spring_id == -1)
			continue;

		Spring* new_spring = all_springs_.at(new_spring_id);
		new_spring->genome.sexually_reproduce(cell->spring_genome, other_cell->spring_genome, true);
	}

	connection_requests.clear();
}

void CellManager::create_weak_offspring(uint32_t parent_id)
{
	// This function creates a new offspring cell for the given parent cell, 
	// and connects them with a spring that is weak and has low oscillation. 
	// The offspring is initialized with some random properties to ensure it doesn't cling too tightly to the parent.
	// This is so when this mutation occours it doesnt throw off the whole organism

	CellBodyPair pair = create_cell();
	if (!pair.is_valid)
		return;

	Cell* parent_cell = all_cells_.at(parent_id);
	Body* parent_body = bodies_->at(parent_cell->body_id_);
	Cell* child_cell = all_cells_.at(pair.cell_id);
	Body* child_body = bodies_->at(pair.body_id);

	// creating a child which doesnt grab on too much
	child_body->copy(parent_body);
	child_body->position_ += Random::rand_vector(10.f, 50.f);

	// setting the child cell properties
	child_cell->randomize();
	child_cell->amplitude = 0.2f;
	child_cell->vertical_shift = 0.5f;

	int32_t spring_id = create_spring(parent_id, pair.cell_id);
	if (spring_id < 0) // potential error here
	{
		std::cout << " birthmanagement.cpp line 190 - bad call\n";
		return;
	}
	Spring* spring = all_springs_.at(spring_id);

	// creating a spring that is firm but doesnt oscilate mutch
	spring->genome.randomize();
	spring->genome.amplitude = 0.2f;
}

void CellManager::apply_matter_birth_requests()
{
	for (const MatterBirthRequest& req : matter_birth_requests)
	{
		Body* body = bodies_->emplace(true, true);
		body->position_ = req.position;

		CellMatter* cell_matter = all_cell_matter_.emplace(true, true); // TODO create a creation Function;
		if (cell_matter == nullptr)
			std::cout << "Failed to create cell matter for birth request\n";

		cell_matter->reset_cell_matter();
		cell_matter->cell_to_matter(body, req.inner_color, req.outer_color);
	}

	matter_birth_requests.clear();
}
