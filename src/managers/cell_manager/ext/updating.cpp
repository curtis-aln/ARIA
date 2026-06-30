#include "../cell_manager.h"

void CellManager::deselect_cell()
{
	selected_cell = nullptr;
	protozoa_tracker_.is_active = false;
}


void CellManager::update()
{
	// if we have a selected cell and it has died, we need to deselect it to avoid null errors
	if (selected_cell == nullptr || selected_cell->should_remove())
	{
		deselect_cell();
	}

	// updating the cells and springs
	update_cells();
	update_springs();
	
	// reproductive system
	collect_reproduction_requests();
	apply_birth_requests();
	apply_connection_requests();

	// death
	handle_death();
}

void CellManager::update_cells()
{
	for (Cell* cell : all_cells_)
	{
		Body* body = bodies_->at(cell->body_id_);
		cell->update_statistics();
		cell->update_organics(body);

	}
}

void CellManager::update_springs()
{
	for (Spring* spring : all_springs_)
	{
		Cell* cell_a = all_cells_.at(spring->cell_A_id);
		Cell* cell_b = all_cells_.at(spring->cell_B_id);
		Body* body_a = bodies_->at(cell_a->body_id_);
		Body* body_b = bodies_->at(cell_b->body_id_);

		if (spring->broken || !bodies_->is_obj_active(body_a->id_) || !bodies_->is_obj_active(body_b->id_))
		{
			if (spring->clock_ > 30)
			{
				all_springs_.remove(spring);
			}
			spring->broken = false;
			continue;
		}

		spring->update_physics(*body_a, *body_b);
		spring->update_organics(*cell_a, *cell_b);
	}
}


void CellManager::update_food_interactions(FoodManager& food_manager)
{
	auto grid = food_manager.spatial_hash_grid;

	for (Cell* cell : all_cells_)
	{
		Body* body = bodies_->at(cell->body_id_);
		if (body == nullptr)
			continue;

		sf::Vector2f pos = body->position_;

		

		FixedSpan<obj_idx> nearby_food_ids{100};
		grid.find(pos.x, pos.y, &nearby_food_ids);

		for (int i = 0; i < nearby_food_ids.count; ++i)
		{
			obj_idx food_id = nearby_food_ids[i];
			Food* food = food_manager.at(food_id);
			if (food == nullptr)
				continue;
			
			Body* food_body = bodies_->at(food->body_id_);

			sf::Vector2f food_pos = food_body->position_;
			float food_radius = food_body->radius_;

			float dist_sq = (food_pos - pos).lengthSquared();
			float rel_rad = food_radius + body->radius_;
			float rel_rad_sq = rel_rad * rel_rad;

			if (dist_sq < rel_rad_sq + 1.f)
			{
				// The cell is in contact with the food
				float transfer = CellSettings::conversion_rate;
				cell->eat(transfer);
				food->nutrients -= transfer;
			}
		}
	}
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


Cell* CellManager::find_cell_at_point(const sf::Vector2f mouse_position, bool make_selected_cell)
{
	// When this function is called it checks every cell in the world to see if the mouse click is within the bounds of any cell. 
	// If it finds a cell that contains the mouse position, it sets that cell as the selected cell and returns true. 
	//  bool make_selected_cell - If true, the cell that is found will be set as the selected cell.
	protozoa_tracker_.is_active = false;
	for (Cell* cell : all_cells_)
	{
		Body* body = bodies_->at(cell->body_id_);
		float dist_sq = (mouse_position - body->position_).lengthSquared();
		if (dist_sq < cell->radius * cell->radius)
		{
			// We tell the cell manager which cell is selected, 
			// so it can be used in other parts of the program (like rendering debug info for the selected cell).
			if (make_selected_cell)
			{
				selected_cell = cell;
				protozoa_tracker_.is_active = true;
			}
			return cell;
		}
	}
	return nullptr;
}

void CellManager::fill_snapshot(SimSnapshot& snapshot)
{
	// Selected cell Logic
	if (selected_cell != nullptr)
	{
		protozoa_tracker_.update_primitive(selected_cell, all_cells_, all_springs_, *bodies_);
	}
	snapshot.protozoa_tracker = protozoa_tracker_;
	snapshot.selected_a_cell = selected_cell != nullptr;

	fill_render_data(snapshot.render);
}

void CellManager::fill_render_data(RenderData& render_data)
{
	const int n = get_cell_count();

	// now we handle springs, we can just store the indexes as then the renderer can read them from the positions container above
	const int spring_count = static_cast<int>(all_springs_.size());
	render_data.spring_connections.resize(spring_count);
	int i = 0;

	for (Spring* spring : all_springs_)
	{
		render_data.spring_connections[i++] = { get_cell_pos(spring->cell_A_id), get_cell_pos(spring->cell_B_id) };
	}
}

const sf::Vector2f* CellManager::get_selected_protozoa_pos() const
{
	// This function returns the position of the selected protozoa, 
	// if there is one.
	if (selected_cell != nullptr)
	{
		Body* body = bodies_->at(selected_cell->body_id_);
		return &body->position_;
	}
	return nullptr;
}

sf::Vector2f& CellManager::get_cell_pos(int cell_id)
{
	return bodies_->at(all_cells_.at(cell_id)->body_id_)->position_;
}

void CellManager::drag_selected_cell_to_point(const sf::Vector2f& target_position, const float move_fraction)
{
	if (selected_cell == nullptr)
		return;

	Body* body = bodies_->at(selected_cell->body_id_);
	body->position_ = target_position;
	
	const sf::Vector2f mouse_pos = m_window_->mapPixelToCoords(sf::Mouse::getPosition(*m_window_));
	const sf::Vector2f diff = mouse_pos - body->position_;
	body->position_ += diff * move_fraction; // apply a small force towards the mouse position
}


CellBodyPair CellManager::create_cell(const sf::Vector2f& position, bool random_genetics)
{
	// This is the safest way to create a cell with a body, all creation events Must go through this function to ensure that the cell and body are linked correctly.
	// if there are not any already avalable cells in the o_vector we create a new one

	// Finding a body
	Body* body = bodies_->emplace(true, true);
	if (body == nullptr)
		return { nullptr, nullptr };

	// Finding a cell
	Cell* cell = all_cells_.emplace(true, true);
	if (cell == nullptr)
	{
		// raise an error as there shouldnt be a situation where we have a body but no cell, this should never happen
		std::cerr << "[ERROR]: Failed to create cell during initialization. Max cells reached.\n";
		bodies_->remove(body);
		return { nullptr, nullptr };
	}

	// resetting the cell just incase it isnt brand new
	cell->reset();

	// connecting the two
	cell->body_id_ = body->id_;
	body->position_ = position;

	if (random_genetics)
	{
		cell->randomize();
	}

	body->radius_ = cell->radius;
	body->mass_ = body->radius_;

	return { cell, body };
}

Spring* CellManager::create_spring(const int cell_a_id, const int cell_b_id)
{
	Spring* spring = all_springs_.emplace(true, true);
	
	if (spring == nullptr)
	{
		return nullptr;
	}
	spring->reset();
	spring->cell_A_id = cell_a_id;
	spring->cell_B_id = cell_b_id;
	return spring;
}