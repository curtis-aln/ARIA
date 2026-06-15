#include "../cell_manager.h"

void CellManager::deselect_cell()
{
	if (selected_cell != nullptr)
	{
		selected_cell = nullptr;
	}
}


void CellManager::update(bool update_physics_only)
{
	for (Spring* spring : all_springs_)
	{
		Cell* cell_a = all_cells_.at(spring->cell_A_id);
		Cell* cell_b = all_cells_.at(spring->cell_B_id);
		Body* body_a = bodies_->at(spring->cell_A_id);
		Body* body_b = bodies_->at(spring->cell_B_id);
		spring->update_physics(*body_a, *body_b);

		if (!update_physics_only)
		{
			spring->update_organics(*cell_a, *cell_b);
		}
	}


	if (!update_physics_only)
	{
		for (Cell* cell : all_cells_)
		{
			Body* body = bodies_->at(cell->body_id_);
			cell->update_statistics();
			cell->update_organics(body);

		}
	}

	//collect_reproduction_requests(all_cells_); todo
	//apply_birth_requests(all_cells_, all_springs_);

	//apply_connection_requests(all_cells_, all_springs_);
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