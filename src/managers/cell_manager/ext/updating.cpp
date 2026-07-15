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

	update_statistics();
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
		// if the spring has broken on its own
		if (spring->is_spring_broken())
		{
			all_springs_.remove(spring);
			continue;
		}

		Cell* cell_a = all_cells_.at(spring->cell_A_id);
		Cell* cell_b = all_cells_.at(spring->cell_B_id);

		// if the spring is connected to a cell that has died, we remove the spring
		if (cell_a->should_remove() || cell_b->should_remove())
		{
			all_springs_.remove(spring);
			continue;
		}

		// otherwise we update the spring physics and organics
		Body* body_a = bodies_->at(cell_a->body_id_);
		Body* body_b = bodies_->at(cell_b->body_id_);

		spring->update_physics(*body_a, *body_b);
		spring->update_organics(*cell_a, *cell_b);
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

void CellManager::fill_snapshot(SimSnapshot& snapshot, sf::FloatRect& visible_bounds)
{
	snapshot.protozoa_tracker = protozoa_tracker_;

	snapshot.world_stats.highlighted_cells = select_indexes.count;

	fill_render_data(snapshot.render, visible_bounds);
}

void CellManager::fill_render_data(RenderData& render_data, sf::FloatRect& visible_bounds)
{
	const int n = get_cell_count();

	// now we handle springs, we can just store the indexes as then the renderer can read them from the positions container above
	const int spring_count = static_cast<int>(all_springs_.size());
	render_data.spring_connections.clear();

	for (Spring* spring : all_springs_)
	{
		Body* body_a = get_cell_body(spring->cell_A_id);
		bool cell_a_visible = visible_bounds.contains({ body_a->position_.x, body_a->position_.y });

		if (!cell_a_visible) continue;

		Body* body_b = get_cell_body(spring->cell_B_id);
		bool cell_b_visible = visible_bounds.contains({ body_b->position_.x, body_b->position_.y });

		if (!cell_b_visible) continue;
		
		const float min_dist = body_a->radius_ + body_b->radius_;

		// The reason why we cant use indexing to fill this array is because we dont know how many bodies are not active,
		// so it messes with the indexing and leads to null connections
		render_data.spring_connections.push_back({ 
			get_cell_pos(spring->cell_A_id), 
			get_cell_pos(spring->cell_B_id),
			min_dist,
			min_dist * 2.f,
			spring->stress });
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

Body* CellManager::get_cell_body(int cell_id)
{
	return bodies_->at(all_cells_.at(cell_id)->body_id_);
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


CellBodyPair CellManager::create_cell(sf::Vector2f position, bool random_genetics)
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
	cell->recreate();

	// resetting the body just incase it isnt brand new
	body->reset();

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

void CellManager::gather_food_in_radius(FixedSpan<cell_idx, uint16_t>& indexes, const sf::Vector2f& position, const float radius)
{
	indexes.clear();

	for (Cell* cell : all_cells_)
	{
		Body* body = bodies_->at(cell->body_id_);
		float dist_sq = (body->position_ - position).lengthSquared();
		if (dist_sq < radius * radius)
		{
			indexes.add(cell->id_);
		}
	}
}

void CellManager::remove_cells_in_radius(const sf::Vector2f& position, const float radius)
{
	gather_food_in_radius(select_indexes, position, radius);

	for (int i = 0; i < select_indexes.count; ++i)
		remove_cell(all_cells_.at(select_indexes[i]));

	for (Spring* spring : all_springs_)
	{
		// making sure we remove the springs that are connected to the cells that have been removed
		Cell* cell_a = all_cells_.at(spring->cell_A_id);
		Cell* cell_b = all_cells_.at(spring->cell_B_id);
		if (all_cells_.is_obj_active(cell_a->id_) == false || all_cells_.is_obj_active(cell_b->id_) == false)
		{
			all_springs_.remove(spring);
		}
	}
}

void CellManager::influence_cell_velocities_in_radii(const sf::Vector2f& position, const float radius, const int intensity)
{
	gather_food_in_radius(select_indexes, position, radius);

	for (int i = 0; i < select_indexes.count; ++i)
	{
		Cell* cell = all_cells_.at(select_indexes[i]);
		Body* body = bodies_->at(cell->body_id_);

		sf::Vector2f direction = (position - body->position_).normalized();
		body->velocity_ += direction * (float)intensity;
	}
}

void CellManager::handle_cell_manager_event(SimCommand& cmd)
{
	switch (cmd.type)
	{
	// Spring Natual Selection Modifier
	case CommandType::SetSpringBreakingForce:
		Spring::SPRING_BREAK_FORCE = cmd.float_val;
		break;
	case CommandType::SetSpringBreakingLength:
		Spring::SPRING_BREAK_LENGTH = cmd.float_val;
		break;
	case CommandType::SetSpringDamageThreshold:
		Spring::SPRING_DAMAGE_THRESH = cmd.float_val;
		break;
	case CommandType::SetSpringWorkConst:
		Spring::SPRING_WORK_CONST = cmd.float_val;
		break;

	case CommandType::SetRadius:
		//    if (selected_protozoa)
		//        selected_protozoa->get_cells()[cmd.cell_spring_idx].radius = cmd.float_val;
		    break;

	case CommandType::SetAmplitude:
	//    if (selected_protozoa)
	//        selected_protozoa->get_cells()[cmd.cell_spring_idx].amplitude = cmd.float_val;
		break;

	case CommandType::SetFrequency:
		//if (selected_protozoa)
		//    selected_protozoa->get_cells()[cmd.cell_spring_idx].frequency = cmd.float_val;
		break;

	case CommandType::SetVerticalShift:
		//if (selected_protozoa)
		//    selected_protozoa->get_cells()[cmd.cell_spring_idx].vertical_shift = cmd.float_val;
		break;

	case CommandType::SetOffset:
		//if (selected_protozoa)
		//    selected_protozoa->get_cells()[cmd.cell_spring_idx].offset = cmd.float_val;
		break;

	case CommandType::MutateProtozoa:
		//if (selected_protozoa)
	   //     selected_protozoa->mutate(cmd.mutate.mut_rate, cmd.mutate.mut_range);
		break;

	case CommandType::AddCell:
		//if (selected_protozoa)
		//    selected_protozoa->add_cell();
		break;

	case CommandType::RemoveCell:
		//if (selected_protozoa)
		//    selected_protozoa->remove_cell();
		break;

	case CommandType::AddSpring:
		//if (selected_protozoa)
		//    selected_protozoa->add_spring(); todo
		break;

	case CommandType::RemoveSpring:
		//if (selected_protozoa)
		//    selected_protozoa->remove_spring(); todo
		break;

	case CommandType::InjectProtozoa:
		//if (selected_protozoa)
		//    m_world_.inject_protozoa(selected_protozoa, cmd.float_val);
		break;

	case CommandType::KillProtozoa:
		//if (selected_protozoa)
		//    selected_protozoa->kill(); // todo
		break;

	case CommandType::ForceReproduce:
		//if (selected_protozoa) // todo
		//    selected_protozoa->force_reproduce();
		break;

	case CommandType::MakeImmortal:
		//if (selected_protozoa) // todo
		//    selected_protozoa->immortal = cmd.bool_val;
		break;

	case CommandType::CloneProtozoa:
		//if (selected_protozoa)
		//{
		//    m_world_.create_offspring(selected_protozoa, false);
		//}
		break;

	case CommandType::SetSpringAmplitude:
		//if (selected_protozoa)
		//{
		//    m_world_.selected_protozoa_->get_springs()[cmd.cell_spring_idx].amplitude = cmd.float_val;
		//}
		break;

	case CommandType::SetSpringFrequency:
		//if (selected_protozoa)
		//{
		//    m_world_.selected_protozoa_->get_springs()[cmd.cell_spring_idx].frequency = cmd.float_val;
		//}
		break;

	case CommandType::SetSpringOffset:
		//if (selected_protozoa)
		//{
		//    m_world_.selected_protozoa_->get_springs()[cmd.cell_spring_idx].offset = cmd.float_val;
		//}
		//break;
			//m_world_.selected_protozoa_->get_springs()[cmd.cell_spring_idx].offset = cmd.float_val;
		//}
		break;

	case CommandType::SetSpringVerticalShift:
		//if (selected_protozoa)
		//{
			//m_world_.selected_protozoa_->get_springs()[cmd.cell_spring_idx].vertical_shift = cmd.float_val;
		//}
		break;

	case CommandType::SetDampingConst:
		//if (selected_protozoa)
		//{
		   // m_world_.selected_protozoa_->get_springs()[cmd.cell_spring_idx].damping = cmd.float_val;
		//}
		break;

	case CommandType::SetSpringConst:
		//if (selected_protozoa)
		//{
		   // m_world_.selected_protozoa_->get_springs()[cmd.cell_spring_idx].spring_const = cmd.float_val;
		//}
		break;

	}
}