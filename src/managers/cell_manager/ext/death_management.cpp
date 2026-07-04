#include "../cell_manager.h"

// The only case in which a cell is removed from the world:
// integrity is zero

void CellManager::handle_death()
{
	for (Cell* cell : all_cells_)
	{
		if (!cell->should_remove() || !bodies_->is_obj_active(cell->body_id_))
			continue;
		
		all_cells_.remove(cell);
		bodies_->remove(cell->body_id_);
	}
}