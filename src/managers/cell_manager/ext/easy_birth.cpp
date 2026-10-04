#include "../cell_manager.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <entities/body.h>
#include <entities/cell/cell.h>
#include <entities/cell/cell_settings.h>
#include <entities/cell/spring_links.h>
#include <entities/spring/spring.h>
#include <iostream>
#include <SFML/System/Vector2.hpp>
#include <unordered_set>
#include <utility>
#include <Utils/random.h>
#include <Utils/spatial_grid/fixed_span.h>
#include <Utils/spatial_grid/simple_spatial_grid.h>
#include <vector>


void CellManager::create_protozoa_from_pool(const sf::Vector2f position, const unsigned max_cells, const unsigned max_springs)
{
	float spawn_radius = CellSettings::spawn_radius * 5.f;

	std::vector<uint32_t> cell_indexes;
	cell_indexes.reserve(max_cells);

	for (unsigned i = 0; i < max_cells; ++i)
	{
		const sf::Vector2f spawn_pos = Random::rand_position_in_circle(position, spawn_radius);
		const CellBodyPair& pair = create_cell(spawn_pos, true);
		if (!pair.is_valid)
			break;
		cell_indexes.push_back(pair.cell_id);
	}

	const size_t n = cell_indexes.size();
	if (n < 2 || max_springs == 0)
		return; // nothing to connect

	// Track which pairs already have a spring, so we never place two springs
	// on the same pair of cells. Pair key: smaller index combined with larger.
	auto pair_key = [](uint32_t a, uint32_t b) -> uint64_t {
		if (a > b) std::swap(a, b);
		return (static_cast<uint64_t>(a) << 32) | b;
		};
	std::unordered_set<uint64_t> used_pairs;
	used_pairs.reserve(max_springs);

	unsigned springs_placed = 0;

	auto try_add_spring = [&](uint32_t cell_a, uint32_t cell_b) -> bool {
		const uint64_t key = pair_key(cell_a, cell_b);
		if (used_pairs.contains(key))
			return false;

		const int32_t result = create_spring(cell_a, cell_b); // adjust to your actual API
		if (result == -1)
			return false;

		Spring* spring = all_springs_.at(result);
		spring->genome.randomize();

		used_pairs.insert(key);
		++springs_placed;
		return true;
		};

	// --- Phase 1: random spanning tree, guarantees every cell is connected ---
	// Shuffle the indexes, then link each new cell to a random *already-connected* cell.
	// This produces a random tree shape rather than a straight chain or star.
	std::vector<uint32_t> shuffled = cell_indexes;
	std::shuffle(shuffled.begin(), shuffled.end(), Random::get_engine());

	for (size_t i = 1; i < shuffled.size() && springs_placed < max_springs; ++i)
	{
		const size_t connect_to = Random::rand_range(size_t(0), i - 1); // random index in [0, i)
		try_add_spring(shuffled[i], shuffled[connect_to]);
	}

	// --- Phase 2: fill remaining spring budget with random extra edges ---
	const unsigned remaining_budget = max_springs - springs_placed;
	if (remaining_budget == 0 || n < 2)
		return;

	// Cap attempts so a nearly-saturated small graph can't spin forever
	// trying to find a pair that isn't already used.
	const unsigned max_attempts = remaining_budget * 10;
	for (unsigned attempt = 0; attempt < max_attempts && springs_placed < max_springs; ++attempt)
	{
		const uint32_t a = cell_indexes[Random::rand_range(size_t(0), n - 1)];
		uint32_t b = cell_indexes[Random::rand_range(size_t(0), n - 1)];
		if (a == b)
			continue;

		try_add_spring(a, b);
	}
}




namespace
{
	constexpr bool log_reproduction_blocks = false; // flip on to see why organisms fail to reproduce

	bool contains(FixedSpan<cell_idx>& ids, const cell_idx id)
	{
		for (int i = 0; i < ids.count; ++i)
			if (ids[i] == id)
				return true;
		return false;
	}

	// The cell at the other end of the spring
	cell_idx other_end_of(const Spring& spring, const cell_idx this_end)
	{
		return (spring.cell_A_id == this_end) ? spring.cell_B_id : spring.cell_A_id;
	}
}

// Walks cell -> spring -> cell from start_id (breadth-first).
// Returns true only if every connected cell is ready to reproduce AND the organism has
// no more than max_protozoa_cells cells.
// On true, organism_cell_ids holds every cell in the organism. On false its contents are unspecified.
bool CellManager::can_protozoa_reproduce(const cell_idx start_id, FixedSpan<cell_idx>& organism_cell_ids)
{
	// Single exit point for every failure: put a breakpoint here to catch them all
	auto blocked = [&](const char* reason, const cell_idx cell_id) {
		if constexpr (log_reproduction_blocks)
			std::cout << "[repro blocked] start=" << start_id << " cell=" << cell_id
			<< " size=" << organism_cell_ids.count << " reason: " << reason << "\n";
		return false;
		};

	organism_cell_ids.clear();
	organism_cell_ids.add(start_id);

	// organism_cell_ids is both the queue and the visited list:
	// cells before next_to_visit are done, cells after it are waiting.
	int next_to_visit = 0;
	while (next_to_visit < organism_cell_ids.count)
	{
		const cell_idx current_cell_id = organism_cell_ids[next_to_visit++];
		const Cell& current_cell = *all_cells_.at(current_cell_id);

		if (!current_cell.should_reproduce())
			return blocked("cell not ready", current_cell_id);

		const SpringLinks& links = current_cell.spring_links_;
		for (uint32_t link = 0; link < links.size_; ++link)
		{
			const uint32_t spring_id = links.connected_spring_ids_[link];
			const Spring& spring = *all_springs_.at(spring_id);

			// the link graph must be consistent: this spring has to point back at this cell
			assert(spring.cell_A_id == current_cell_id || spring.cell_B_id == current_cell_id);

			const cell_idx neighbour_id = other_end_of(spring, current_cell_id);

			if (contains(organism_cell_ids, neighbour_id))
				continue;

			if (organism_cell_ids.count >= max_protozoa_cells)
				return blocked("organism too large", neighbour_id);

			organism_cell_ids.add(neighbour_id);
		}
	}

	return true;
}

// Clones the organism in parent_ids (output of can_protozoa_reproduce) next to the original.
// Returns false if a pool ran out mid-clone; the partial clone is killed and the death pass cleans it up.
bool CellManager::protozoa_reproduce(FixedSpan<cell_idx>& parent_ids)
{
	const int cell_count = parent_ids.count;
	if (cell_count <= 0 || cell_count > max_protozoa_cells)
		return false;

	const sf::Vector2f offset = calculate_clone_offset(parent_ids);

	std::array<cell_idx, max_protozoa_cells> offspring_ids{}; // offspring_ids[i] is the clone of parent_ids[i]
	int cells_created = 0;
	bool success = true;

	auto index_of_parent = [&](uint32_t cell_id) -> int {
		for (int i = 0; i < cell_count; ++i)
			if (parent_ids[i] == cell_id)
				return i;
		return -1;
		};

	// cells
	for (; cells_created < cell_count; ++cells_created)
	{
		const int32_t offspring_id = clone_cell(parent_ids[cells_created], offset);
		if (offspring_id == -1)
		{
			success = false;
			break;
		}
		offspring_ids[cells_created] = static_cast<cell_idx>(offspring_id);
	}


	// springs
	for (int cell_index = 0; success && cell_index < cell_count; ++cell_index)
	{
		const cell_idx parent_cell_id = parent_ids[cell_index];
		const SpringLinks links = all_cells_.at(parent_cell_id)->spring_links_; // copy, so we iterate a stable snapshot

		for (uint32_t link = 0; success && link < links.size_; ++link)
		{
			const uint32_t parent_spring_id = links.connected_spring_ids_[link];
			const Spring* parent_spring = all_springs_.at(parent_spring_id);

			// each spring is seen from both of its cells, so only handle it from the A side
			if (parent_spring->cell_A_id != parent_cell_id)
				continue;

			// a spring leading outside the organism has nothing to connect to
			const int other_index = index_of_parent(parent_spring->cell_B_id);
			if (other_index < 0)
				continue;

			success = clone_spring(parent_spring_id, offspring_ids[cell_index], offspring_ids[other_index]);
		}
	}

	// rollback: killing the cells is enough, their springs die in death_update
	if (!success)
		for (int i = 0; i < cells_created; ++i)
			all_cells_.at(offspring_ids[i])->kill();

	return success;
}


// ───────────── placement helpers ─────────────

sf::Vector2f CellManager::calculate_organism_centre(FixedSpan<cell_idx>& cell_ids)
{
	sf::Vector2f sum{ 0.f, 0.f };
	for (int i = 0; i < cell_ids.count; ++i)
		sum += get_cell_pos(cell_ids[i]);

	return sum / static_cast<float>(cell_ids.count);
}

// Distance from the centre to the outer edge of the furthest cell
float CellManager::calculate_organism_radius(FixedSpan<cell_idx>& cell_ids, const sf::Vector2f& centre)
{
	float radius = 0.f;
	for (int i = 0; i < cell_ids.count; ++i)
	{
		const Body* body = get_cell_body(cell_ids[i]);
		radius = std::max(radius, (body->position_ - centre).length() + body->radius_);
	}
	return radius;
}

// Random direction, scaled so the clone's bounding circle just clears the original's.
// Every cell is shifted by this same offset, so the clone keeps the original's shape.
sf::Vector2f CellManager::calculate_clone_offset(FixedSpan<cell_idx>& parent_ids)
{
	constexpr float two_pi = 6.28318530718f;
	constexpr float gap_between_organisms = 2.f;

	const sf::Vector2f centre = calculate_organism_centre(parent_ids);
	const float organism_radius = calculate_organism_radius(parent_ids, centre);

	const float angle = Random::rand01_float() * two_pi;
	const sf::Vector2f direction{ std::cos(angle), std::sin(angle) };

	return direction * (organism_radius * 2.f + gap_between_organisms);
}


// ───────────── cloning helpers ─────────────

// Creates one offspring for parent_cell_id at the parent's position + offset. Returns its id, or -1 if the pool is full.
int32_t CellManager::clone_cell(const cell_idx parent_cell_id, const sf::Vector2f& offset)
{
	const CellBodyPair pair = create_cell();
	if (!pair.is_valid)
		return -1;

	// fetch pointers AFTER create_cell in case the pools moved
	Cell* parent_cell = all_cells_.at(parent_cell_id);
	Body* parent_body = bodies_->at(parent_cell->body_id_);
	Cell* offspring_cell = all_cells_.at(pair.cell_id);
	Body* offspring_body = bodies_->at(pair.body_id);

	// energy split, genetics copy, mutation, generation++, parent cooldown reset
	parent_cell->create_offspring(parent_body, offspring_cell, offspring_body, true);

	// create_offspring scatters the child randomly; override with the rigid offset
	offspring_body->position_ = parent_body->position_ + offset;
	offspring_body->velocity_ = { 0.f, 0.f };

	// stop the newborn auto-connect logic from adding extra springs; we build them ourselves
	offspring_cell->new_connections_made = Cell::max_cell_connections;

	parent_cell->turn_off_reproduction();

	return static_cast<int32_t>(pair.cell_id);
}

// Creates a spring between the two offspring cells, with the parent spring's genome copied and mutated.
bool CellManager::clone_spring(const uint32_t parent_spring_id, const cell_idx offspring_cell_a, const cell_idx offspring_cell_b)
{
	const int32_t offspring_spring_id = create_spring(offspring_cell_a, offspring_cell_b);
	if (offspring_spring_id == -1)
		return false;

	// re-fetch after create_spring
	Spring* parent_spring = all_springs_.at(parent_spring_id);
	Spring* offspring_spring = all_springs_.at(offspring_spring_id);

	parent_spring->create_offspring(*offspring_spring);
	offspring_spring->genome.mutate();
	return true;
}