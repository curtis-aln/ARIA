#include "../cell_manager.h"
#include <cmath>
#include <cstdint>
#include <entities/cell/cell.h>
#include <entities/matter/CellMatter.h>
#include <entities/spring/tendon.h>
#include <iostream>
#include <ostream>
#include <SFML/System/Vector2.hpp>
#include <Utils/spatial_grid/simple_spatial_grid.h>

// The only case in which a cell is removed from the world:
// integrity is zero



// ---------------------------- collect death requests ----------------------------
void CellManager::collect_tendon_death_requests()
{
	tendon_death_requests.clear();
	for (Tendon* tendon : all_tendons_)
	{
		if (tendon->is_tendon_broken())
			tendon_death_requests.push_back(tendon->id_);
	}
}

void CellManager::collect_matter_death_requests()
{
	for (CellMatter* matter : all_cell_matter_)
	{
		if (!matter->dead || !bodies_->is_obj_active(matter->body_id_))
			continue;

		matter_death_requests_.push_back(matter->id_);
	}
}

void CellManager::collect_cell_death_requests()
{
	for (Cell* cell : all_cells_)
	{
		if (!cell->is_alive() || !bodies_->is_obj_active(cell->body_id_))
			cell_death_requests_.push_back(cell->id_);
	}
}

// ---------------------------- misc ----------------------------


void CellManager::speed_tax_cell(Cell* cell)
{
	if (cell->immortal_ || statistics_.min_speed <= 0.f)
		return;

	sf::Vector2f velocity = bodies_->at(cell->body_id_)->get_vel();
	float speed_sq = velocity.x * velocity.x + velocity.y * velocity.y;
	if (speed_sq < statistics_.min_speed * statistics_.min_speed)
	{
		float deficit_ratio = 1.f - (sqrt(speed_sq) / statistics_.min_speed); // 0 at threshold, 1 at rest
		cell->change_energy(-abs(speed_energy_tax * deficit_ratio)); // guaranteed to be negative, so we use abs() to avoid confusion
	}
}

// ---------------------------- remove entities ----------------------------


void CellManager::remove_cell(cell_idx cell_id)
{
	Cell* cell = all_cells_.at(cell_id);
	if (cell == nullptr) return;
	register_death_stat(cell->internal_clock_, cell->offspring_count > 0);

	cell->kill();

	// removing all the tendons attached to this cell
	for (int i = 0; i < cell->tendon_links_.size_; ++i)
	{
		uint32_t tendon_id = cell->tendon_links_.connected_tendon_ids_[i];
		Tendon* tendon = all_tendons_.at(tendon_id);
		if (tendon == nullptr) continue;
		tendon->break_tendon();
	}

	// removing the cell and body from their respective containers
	all_cells_.remove(cell);
	bodies_->remove(cell->body_id_);
}

void CellManager::remove_cell_matter(cell_idx matter_id)
{
	CellMatter* matter = all_cell_matter_.at(matter_id);
	if (matter == nullptr) return;
	all_cell_matter_.remove(matter);
	bodies_->remove(matter->body_id_);
}

void CellManager::remove_tendon(uint32_t tendon_id)
{
	Tendon* tendon = all_tendons_.at(tendon_id);
	if (tendon == nullptr) return;

	// all tendons are connected to cells, when the cells die they automatically remove the tendons.
	// due to this we dont know whever the tendon is connected to a cell that is still alive or not
	// However there is no harm in trying to remove a tendon from a cell that is already dead, so we do it anyway
	Cell* cellA = all_cells_.at(tendon->cell_A_id);
	Cell* cellB = all_cells_.at(tendon->cell_B_id);

	for (Cell* cell : { cellA, cellB })
	{
		cell->tendon_links_.remove_connection(tendon->id_);
	}

	// resetting the tendon so it can be reused
	tendon->reset();
	all_tendons_.remove(tendon);
}

// ---------------------------- Apply death requests ----------------------------
void CellManager::apply_cell_death_requests()
{
	for (cell_idx cell_id : cell_death_requests_)
	{
		sf::Vector2f pos = get_cell_pos(cell_id);
		Cell* cell = all_cells_.at(cell_id);
		matter_birth_requests.push_back({ pos, cell->get_inner_color(), cell->get_outer_color() });
		remove_cell(cell_id);
	}
	cell_death_requests_.clear();
}

void CellManager::apply_matter_death_requests()
{
	for (cell_idx matter_id : matter_death_requests_)
	{
		remove_cell_matter(matter_id);
	}
	matter_death_requests_.clear();
}

void CellManager::apply_tendon_death_requests()
{
	for (uint32_t tendon_id : tendon_death_requests)
	{
		remove_tendon(tendon_id);
	}
	tendon_death_requests.clear();
}


void CellManager::debug_check_tendon_death_request_duplicates()
{
	// checks the tendon_death_requests vector for duplicates, which should never happen. If it does, it indicates a bug in the code
	for (size_t i = 0; i < tendon_death_requests.size(); ++i)
	{
		for (size_t j = i + 1; j < tendon_death_requests.size(); ++j)
		{
			if (tendon_death_requests[i] == tendon_death_requests[j])
			{
				std::cout << "Duplicate tendon death request found: " << tendon_death_requests[i] << std::endl;
			}
		}
	}
}