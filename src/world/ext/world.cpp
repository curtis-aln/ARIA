#include "../world.h"

#include "../../entities/cell/cell_genome.h"
#include <entities/body.h>
#include <entities/cell/cell.h>
#include <entities/food/food.h>
#include <entities/spring/spring.h>
#include <managers/cell_manager/organism_tracker.h>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <simulation/context/sim_command.h>
#include <simulation/context/sim_snapshot.h>
#include <simulation/context/state.h>
#include <Utils/Graphics/pheromone_grid.h>
#include <Utils/o_vec/o_vec_snapshot.h>
#include <world/world_border.h>
#include <world/world_settings.h>

World::World(sf::RenderWindow* window) : m_window_(window)
{
	food_manager_.create_food_pool(food_manager_.initial_food);
	bound_bodies();

	max_bounds_radius = bounds_radius;
}


void World::render(const SimSnapshot& snapshot, const sf::Vector2f mouse_pos)
{
	world_renderer_.render(snapshot, mouse_pos);
	//cell_manager_.new_born_cell_grid_renderer_.render(*m_window_, mouse_pos, 800.f);
}

void World::reset_world()
{
	// Resetting the food manager
	food_manager_.reset_food_manager();
	// Resetting the cell manager
	cell_manager_.reset_cell_manager();

	// Resetting the world information and Statistics
	statistics_ = WorldStatistics{};
	world_toggles = WorldToggles{};
}

bool World::handle_mouse_click(const sf::Vector2f mouse_position)
{
	return cell_manager_.find_cell_at_point(mouse_position, true) != nullptr;
}

void World::keyboardEvents(const sf::Keyboard::Key& event_key_code)
{
	CellManagerToggles& cell_toggles = cell_manager_.toggles_;
	switch (event_key_code)
	{
	case sf::Keyboard::Key::G: // Toggle the drawing of the cell grid
		world_toggles.draw_collision_grid = !world_toggles.draw_collision_grid;
		break;

	case sf::Keyboard::Key::C: // Show collisions if not in debug mode else show connections between cells
		if (world_toggles.debug_mode)
			cell_toggles.show_connections = !cell_toggles.show_connections;
		else
			world_toggles.toggle_collisions = !world_toggles.toggle_collisions;
		break;

	case sf::Keyboard::Key::F: // Toggle the drawing of the food grid
		world_toggles.draw_food_grid = !world_toggles.draw_food_grid;
		break;

	case sf::Keyboard::Key::D: // Toggle debug mode
		world_toggles.debug_mode = !world_toggles.debug_mode;
		break;

	case sf::Keyboard::Key::T: // Toggle the tracking of statistics, in some cases can speed up simulation
		world_toggles.track_statistics = !world_toggles.track_statistics;
		break;

	case sf::Keyboard::Key::K: // This hides the graphical circles of the selected organism but still shows stats on its body
		if (world_toggles.debug_mode)
			cell_toggles.skeleton_mode = !cell_toggles.skeleton_mode;
		break;

	case sf::Keyboard::Key::B: // Toggle the drawing of bounding boxes around protozoa, only works in debug mode
		if (world_toggles.debug_mode)
			cell_toggles.show_bounding_boxes = !cell_toggles.show_bounding_boxes;
		break;

	default:
		break;
	}
}

void World::handle_right_click(WorldBorder& spawn_area)
{
	const auto& center = spawn_area.center_;
	const float rad = statistics_.mouse_radius;
	const float intensity = statistics_.mouse_intensity;
	const bool  do_cells = world_toggles.mouse_rem_cells;
	const bool  do_food = world_toggles.mouse_rem_food;


	switch (statistics_.mouse_mode)
	{
	case -1: // drag selected cell
		cell_manager_.find_cell_at_point(center, true);
		dragging = true;
		break;


	case 0: // Add
		for (int i = 0; i < static_cast<int>(intensity); i++)
		{
			if (do_cells) cell_manager_.create_new_protozoa(1, &spawn_area);
			if (do_food)  food_manager_.create_food_pool(1, &spawn_area);
		}
		break;

	case 1: // Remove
		if (do_cells) cell_manager_.remove_cells_in_radius(center, rad);
		if (do_food)  food_manager_.remove_food_in_area(center, rad);
		break;

	case 2: // Attract
		if (do_cells) cell_manager_.influence_cell_velocities_in_radii(center, rad, intensity);
		if (do_food)  food_manager_.influence_food_velocities_in_radii(center, rad, intensity);
		break;

	case 3: // Repel
		if (do_cells) cell_manager_.influence_cell_velocities_in_radii(center, rad, -intensity);
		if (do_food)  food_manager_.influence_food_velocities_in_radii(center, rad, -intensity);
		break;
	}
}


void World::fill_snapshot(SimSnapshot& snapshot)
{
	sf::FloatRect visible_bounds = calulcate_visible_range();
	snapshot.render.clear_render_data();

	if (!cell_manager_.toggles_.show_only_newborns && world_toggles.render_food)
		food_manager_.update_position_data(snapshot.render, statistics_.iterations_);
	cell_manager_.update_position_container(snapshot.render, visible_bounds, cell_manager_.toggles_.show_only_newborns, world_toggles.render_cells, world_toggles.render_cells_matter);

	/* Data that goes to both the renderer and the ImGUI panels */
	snapshot.cell_manager_stats = cell_manager_.get_statistics();
	snapshot.world_stats = get_statistics(); // world statistics
	snapshot.food_manager_stats = food_manager_.get_statistics();

	snapshot.world_toggles = world_toggles;
	snapshot.food_toggles = food_manager_.toggles_;
	snapshot.cell_toggles = cell_manager_.toggles_;

	copy_render_data_to_snapshot(snapshot); // render data

	snapshot.world_stats.highlighted_food = food_manager_.selected_cells_indexes_.count;
	cell_manager_.fill_snapshot(snapshot, visible_bounds); // protozoa data

	copy_spatial_grids_to_snapshot(snapshot);

	// Pheromone grid data
	const PheromoneGrid* pheromone_grid = food_manager_.get_pheromone_grid();
	sf::Vector2f grid_size = pheromone_grid->get_grid_size();
	snapshot.food_manager_stats.pheromone_grid_cells_x = grid_size.x;
	snapshot.food_manager_stats.pheromone_grid_cells_y = grid_size.y;
}


void World::copy_render_data_to_snapshot(SimSnapshot& snapshot)
{
	RenderData& render_data = snapshot.render;

	snapshot.cell_manager_stats.cell_count = cell_manager_.get_cell_count();

	render_data.body_debug_snapshot = FillSnapshot<Body>(dbg_bodies_, "Body", sf::Color::White);
	render_data.food_debug_snapshot = FillSnapshot<Food>(dbg_food_, "Food", sf::Color::White);
	render_data.spring_debug_snapshot = FillSnapshot<Spring>(dbg_springs_, "Spring", sf::Color::White);
	render_data.cell_debug_snapshot = FillSnapshot<Cell>(dbg_cells_, "Cell", sf::Color::White);
}

void World::copy_spatial_grids_to_snapshot(SimSnapshot& snapshot)
{
	// printing if there is a protozoa selected
	auto* cell_grid = get_spatial_grid();
	auto* food_grid = &food_eat_resolver_.get_spatial_grid();

	snapshot.food_grid = get_grid_data(food_grid);
	snapshot.cell_grid = get_grid_data(cell_grid);

	if (world_toggles.track_spatial_grids)
	{
		calculate_spatial_grid_statistics(food_grid, snapshot.food_grid);
		calculate_spatial_grid_statistics(cell_grid, snapshot.cell_grid);
	}
}

int World::check_mouse_press(const OrganismTracker& protozoa, const sf::Vector2f mousePosition, const bool tolerance_check) const
{
	/* Check if the mouse is pressing on any cell returns -1 if no cell is pressed */
	for (const Cell& cell : protozoa.cells)
	{
		const Body* body = bodies_.at(cell.body_id_);
		const float dist_sq = (body->position_ - mousePosition).lengthSquared();

		const float rad = body->radius_ * cell_press_tollarance_factor;
		if (dist_sq < rad * rad)
			return cell.body_id_;
	}

	return -1;
}

void World::handle_world_event(SimCommand& cmd)
{
	switch (cmd.type)
	{
	case CommandType::SetWorldToggles:
		world_toggles = cmd.world_toggles;
		break;

	case CommandType::ResetSimulation:
		reset_world();
		break;

	case CommandType::SetWorldRadius:
		bounds_radius = cmd.float_val;
		//world_circular_bounds_.set_radius(bounds_radius);
		//world_rect_bounds_ = { { 0.f, 0.f },{ bounds_radius * 2.f, bounds_radius * 2.f } };
		world_renderer_.world_border_renderer_.setRadius(bounds_radius);
		world_renderer_.world_border_renderer_.setPosition({max_bounds_radius - bounds_radius, max_bounds_radius - bounds_radius});
		break;

	case CommandType::SetInfluenceRadius:
		statistics_.mouse_radius = cmd.float_val;
		break;

	case CommandType::SetMouseIntensity:
		statistics_.mouse_intensity = cmd.int_val;
		break;

	case CommandType::SetSubIterations:
		WorldSettings::tick_sim_multiplier = cmd.int_val;
		break;

	case CommandType::SetCellGridResolution:
		//m_world_.get_spatial_grid()->change_cell_dimsensions(cmd.int_val, cmd.int_val);
		//m_world_.update_spatial_renderers();
		break;

	case CommandType::SetFoodGridResolution:
		//m_world_.get_food_spatial_grid()->change_cell_dimsensions(cmd.int_val, cmd.int_val);
		//m_world_.update_spatial_renderers();
		break;
	}

}
