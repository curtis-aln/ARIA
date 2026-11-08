#include "../cell_manager.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <entities/body/body.h>
#include <entities/cell/cell.h>
#include <entities/cell/cell_settings.h>
#include <entities/cell/tendon_links.h>
#include <entities/spring/tendon.h>
#include <iostream>
#include <SFML/System/Vector2.hpp>
#include <unordered_set>
#include <utility>
#include <Utils/random.h>
#include <Utils/spatial_grid/fixed_span.h>
#include <Utils/spatial_grid/simple_spatial_grid.h>
#include <vector>


void CellManager::create_protozoa_from_pool(const sf::Vector2f position, const unsigned max_cells, const unsigned max_tendons)
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
	if (n < 2 || max_tendons == 0)
		return; // nothing to connect

	// Track which pairs already have a tendon, so we never place two tendons
	// on the same pair of cells. Pair key: smaller index combined with larger.
	auto pair_key = [](uint32_t a, uint32_t b) -> uint64_t {
		if (a > b) std::swap(a, b);
		return (static_cast<uint64_t>(a) << 32) | b;
		};
	std::unordered_set<uint64_t> used_pairs;
	used_pairs.reserve(max_tendons);

	unsigned tendons_placed = 0;

	auto try_add_tendon = [&](uint32_t cell_a, uint32_t cell_b) -> bool {
		const uint64_t key = pair_key(cell_a, cell_b);
		if (used_pairs.contains(key))
			return false;

		const int32_t result = create_tendon(cell_a, cell_b); // adjust to your actual API
		if (result == -1)
			return false;

		Tendon* tendon = all_tendons_.at(result);
		tendon->genome.sexually_reproduce(all_cells_.at(cell_a)->tendon_genome,
			all_cells_.at(cell_b)->tendon_genome);

		used_pairs.insert(key);
		++tendons_placed;
		return true;
		};

	// --- Phase 1: random spanning tree, guarantees every cell is connected ---
	// Shuffle the indexes, then link each new cell to a random *already-connected* cell.
	// This produces a random tree shape rather than a straight chain or star.
	std::vector<uint32_t> shuffled = cell_indexes;
	std::shuffle(shuffled.begin(), shuffled.end(), Random::get_engine());

	for (size_t i = 1; i < shuffled.size() && tendons_placed < max_tendons; ++i)
	{
		const size_t connect_to = Random::rand_range(size_t(0), i - 1); // random index in [0, i)
		try_add_tendon(shuffled[i], shuffled[connect_to]);
	}

	// --- Phase 2: fill remaining tendon budget with random extra edges ---
	const unsigned remaining_budget = max_tendons - tendons_placed;
	if (remaining_budget == 0 || n < 2)
		return;

	// Cap attempts so a nearly-saturated small graph can't spin forever
	// trying to find a pair that isn't already used.
	const unsigned max_attempts = remaining_budget * 10;
	for (unsigned attempt = 0; attempt < max_attempts && tendons_placed < max_tendons; ++attempt)
	{
		const uint32_t a = cell_indexes[Random::rand_range(size_t(0), n - 1)];
		uint32_t b = cell_indexes[Random::rand_range(size_t(0), n - 1)];
		if (a == b)
			continue;

		try_add_tendon(a, b);
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

	// The cell at the other end of the tendon
	cell_idx other_end_of(const Tendon& tendon, const cell_idx this_end)
	{
		return (tendon.cell_A_id == this_end) ? tendon.cell_B_id : tendon.cell_A_id;
	}
}

// Walks cell -> tendon -> cell from start_id (breadth-first).
// Returns true only if every connected cell is ready to reproduce AND the organism has
// no more than max_protozoa_cells cells.
// On true, organism_cell_ids holds every cell in the organism. On false its contents are unspecified.
bool CellManager::can_protozoa_reproduce(const cell_idx start_id, FixedSpan<cell_idx>& organism_cell_ids)
{

	auto blocked = [&](const char* reason, const cell_idx cell_id) {
		if constexpr (log_reproduction_blocks)
			std::cout << "[repro blocked] start=" << start_id << " cell=" << cell_id
			<< " size=" << organism_cell_ids.count << " reason: " << reason << "\n";
		return false;
		};

	organism_cell_ids.clear();
	organism_cell_ids.add(start_id);

	float quorum_sum = 0.f;
	int ready = 0;
	int next_to_visit = 0;
	while (next_to_visit < organism_cell_ids.count)
	{
		const cell_idx current_cell_id = organism_cell_ids[next_to_visit++];
		const Cell& current_cell = *all_cells_.at(current_cell_id);

		if (current_cell.should_reproduce())
			++ready;                                   // was: return blocked("cell not ready")

		quorum_sum += current_cell.quorum_fraction;

		const TendonLinks& links = current_cell.tendon_links_;
		for (uint32_t link = 0; link < links.size_; ++link)
		{
			const uint32_t tendon_id = links.connected_tendon_ids_[link];
			const Tendon& tendon = *all_tendons_.at(tendon_id);
			assert(tendon.cell_A_id == current_cell_id || tendon.cell_B_id == current_cell_id);

			const cell_idx neighbour_id = other_end_of(tendon, current_cell_id);
			if (contains(organism_cell_ids, neighbour_id))
				continue;

			if (organism_cell_ids.count >= max_protozoa_cells)
				return blocked("organism too large", neighbour_id);

			organism_cell_ids.add(neighbour_id);
		}
	}

	// the whole organism has to be walked before this can be judged
	const int   n = organism_cell_ids.count;
	const float quorum = quorum_sum / n;
	const int   needed = std::max(1, static_cast<int>(std::ceil(quorum * n)));
	if (ready < needed)
		return blocked("quorum not met", start_id);
	return true;
}

// Clones the organism in parent_ids (output of can_protozoa_reproduce) next to the original.
// Returns false if a pool ran out mid-clone; the partial clone is killed and the death pass cleans it up.
void CellManager::protozoa_reproduce(FixedSpan<cell_idx>& parent_ids)
{
	const int cell_count = parent_ids.count;
	if (cell_count <= 0 || cell_count > max_protozoa_cells)
		return;

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

		// turning off the seek_newborn_links_ flag so the new cell does not try to connect to other newborns
		Cell* offspring_cell = all_cells_.at(offspring_id);
		offspring_cell->seek_newborn_links_ = false;
	}


	// tendons
	for (int cell_index = 0; success && cell_index < cell_count; ++cell_index)
	{
		const cell_idx parent_cell_id = parent_ids[cell_index];
		const TendonLinks links = all_cells_.at(parent_cell_id)->tendon_links_; // copy, so we iterate a stable snapshot

		for (uint32_t link = 0; success && link < links.size_; ++link)
		{
			const uint32_t parent_tendon_id = links.connected_tendon_ids_[link];
			const Tendon* parent_tendon = all_tendons_.at(parent_tendon_id);

			// each tendon is seen from both of its cells, so only handle it from the A side
			if (parent_tendon->cell_A_id != parent_cell_id)
				continue;

			// a tendon leading outside the organism has nothing to connect to
			const int other_index = index_of_parent(parent_tendon->cell_B_id);
			if (other_index < 0)
				continue;

			success = clone_tendon(parent_tendon_id, offspring_ids[cell_index], offspring_ids[other_index]);
		}
	}

	apply_structural_mutations(offspring_ids, cells_created);

}

void CellManager::apply_structural_mutations(std::array<cell_idx, max_protozoa_cells>& cell_ids, int cell_count)
{
	for (int i = 0; i < cell_count; ++i)
	{
		Cell* cell = all_cells_.at(cell_ids[i]);

		// cell creation chance
		if (Random::rand01_float() < cell->add_cell_chance)
		{
			create_weak_offspring(cell->id_);
		}

		if (Random::rand01_float() < cell->add_tendon_chance)
		{
			add_tendon_mutation(cell_ids, cell_count, cell);
		}

		if (Random::rand01_float() < cell->remove_cell_chance)
		{
			cell->kill();
		}

		if (Random::rand01_float() < cell->remove_tendon_chance)
		{
			auto& links = cell->tendon_links_;
			if (links.size_ == 0)
				continue;
			const uint32_t idx = Random::rand_range(uint32_t(0), links.size_ - 1);
			all_tendons_.at(links.connected_tendon_ids_[idx])->break_tendon();

		}
	}
}

void CellManager::add_tendon_mutation(std::array<cell_idx, max_protozoa_cells>& cell_ids, int cell_count, const Cell* cell)
{
	// pick a random cell from the cell_ids
	Cell* other_cell = all_cells_.at(cell_ids[Random::rand_range(0, cell_count - 1)]);

	if (other_cell == nullptr)
	{
		std::cout << "easy_birth.cpp other_cell creation was flagged as a nullptr\n";
	}

	// check if we have already connected to the cell
	if (other_cell->id_ == cell->id_ || cell->already_connected_to(other_cell->id_))
		return;

	// if not, create a tendon between the two cells
	const int32_t spring_id = create_tendon(cell->id_, other_cell->id_);
	if (spring_id == -1)
	{
		return; // tendon links is full
	}

	Tendon* spring = all_tendons_.at(spring_id);

	// give the tendon the appropriate genetics
	spring->genome.sexually_reproduce(cell->tendon_genome, other_cell->tendon_genome);
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
		radius = std::max(radius, (body->get_pos() - centre).length() + body->get_radius());
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
	offspring_body->set_pos(parent_body->get_pos() + offset);
	offspring_body->set_vel({ 0.f, 0.f });

	// stop the newborn auto-connect logic from adding extra springs; we build them ourselves
	// offspring_cell->new_connections_made = Cell::max_cell_connections; Todo // BUG

	parent_cell->turn_off_reproduction();

	return static_cast<int32_t>(pair.cell_id);
}

// Creates a tendon between the two offspring cells, with the parent tendon's genome copied and mutated.
bool CellManager::clone_tendon(const uint32_t parent_tendon_id, const cell_idx offspring_cell_a, const cell_idx offspring_cell_b)
{
	const int32_t offspring_tendon_id = create_tendon(offspring_cell_a, offspring_cell_b);
	if (offspring_tendon_id == -1)
		return false;

	// re-fetch after create_tendon
	Tendon* parent_tendon = all_tendons_.at(parent_tendon_id);
	Tendon* offspring_tendon = all_tendons_.at(offspring_tendon_id);

	TendonGenome& genome_a = all_cells_.at(offspring_cell_a)->tendon_genome;
	TendonGenome& genome_b = all_cells_.at(offspring_cell_b)->tendon_genome;
	offspring_tendon->genome.sexually_reproduce(genome_a, genome_b);
	return true;
}