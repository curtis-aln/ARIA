#include "../cell_manager.h"

// The only case in which a cell is removed from the world:
// integrity is zero

void CellManager::handle_death()
{
	for (Cell* cell : all_cells_)
	{
		if (cell->is_alive() || cell->integrity > 0)
			continue;
		
		all_cells_.remove(cell);
		bodies_->remove(cell->body_id_);
		cell->reset();
	}
}