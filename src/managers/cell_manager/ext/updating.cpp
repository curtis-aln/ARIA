#include "../cell_manager.h"

void CellManager::deselect_cell()
{
	if (selected_cell != nullptr)
	{
		selected_cell = nullptr;
	}
}


void CellManager::update_all_cells()
{
	for (Spring* spring : all_springs_)
	{
		Cell* cell_a = all_cells_.at(spring->cell_A_id);
		Cell* cell_b = all_cells_.at(spring->cell_B_id);
		spring->update(*cell_a, *cell_b);
	}

	for (Cell* cell : all_cells_)
	{
		cell->update();
	}

	//collect_reproduction_requests(all_cells_);
	//apply_birth_requests(all_cells_, all_springs_);

	//apply_connection_requests(all_cells_, all_springs_);
}


void CellManager::update_cell_collisions() const
{
	int idx = 0;

	for (Cell* cell : all_cells_)
		cell->accelerate(collision_resolutions[idx++]);
}

void CellManager::check_for_extinction_event()
{
	// if protozoas are still alivee or if auto reset on extinction is disabled, we dont need to do anything
	if (all_cells_.size() > 0 || !auto_reset_on_extinction)
		return;

	std::cout << "Extinction event occurred, respawning initial protozoa...\n";

	//for (unsigned i = 0; i < initial_protozoa; ++i)
	//{
	//	Protozoa* protozoa = all_protozoa_.add();
	//	build_protozoa(*protozoa, world_bounds, false);
	//}



}

void CellManager::bound_cell(Cell* cell)
{
	const sf::Vector2f center = world_bounds_->center_;
	sf::Vector2f& position = cell->get_pos();
	const float cell_radius = cell->radius;
	const float world_radius = world_bounds_->bounds_radius;

	const sf::Vector2f direction = position - center;
	const float dist_sq = (position - center).lengthSquared();
	const float bounds_radius = world_radius - cell_radius - cell_radius * 0.1f; // Ensure the entire circle stays inside
	const float bounds_radius_sq = bounds_radius * bounds_radius;

	if (dist_sq > bounds_radius_sq) // Outside the boundary
	{
		const float dist = std::sqrt(dist_sq);
		sf::Vector2f normal = direction / dist; // Normalize direction

		// Move the circle back inside
		position = center + normal * bounds_radius;

		// Apply velocity adjustment to prevent escaping
		const float k = world_bounds_->border_repulsion_magnitude;
		const float diff = dist - bounds_radius;
		cell->accelerate(-normal * k * diff);

	}
}