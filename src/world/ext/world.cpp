#include "../world.h"

#include <algorithm>
#include "../../Utils/utility_SFML.h"
#include "../../Utils/Graphics/CircleBatchRenderer.h"
#include "../../entities/cell/cell_genome.h"
#include "simulation/settings/settings.h"

thread_local FixedSpan<uint32_t> World::tl_nearby_ids{25};
thread_local FixedSpan<obj_idx> World::tl_nearby_food{25};



World::World(sf::RenderWindow* window)
    : m_window_(window), world_border_renderer_(make_circle(world_circular_bounds_.bounds_radius, world_circular_bounds_.center_))
{
    claim_buffer.reserve(FoodManagerSettings::max_food);

    init_circle_renderers();
    init_food_jobs();
    init_collision_jobs();
    cell_manager_.init_protozoa_container();
    food_manager_.init();


    render_data_.reserve(static_cast<int>(max_circles));
    distribution_.reserve(max_circles);
    inner_radii_.resize(max_circles);

    velocity_resolutions.resize(max_circles);
    collision_resolutions.resize(max_circles);
}

void World::init_circle_renderers()
{
	outer_circle_renderer_.init(m_window_, tex_rad, CellManagerSettings::max_protozoa);
	inner_circle_renderer_.init(m_window_, tex_rad, CellManagerSettings::max_protozoa);

	const int max_entities = CellManagerSettings::max_protozoa + FoodManagerSettings::max_food;
    entity_positions_.reserve(max_entities);
	entity_velocities_.reserve(max_entities);
	entity_radii_.reserve(max_entities);
}


void World::render(const SimSnapshot& snapshot, Font* font, const sf::Vector2f mouse_pos)
{
    if (snapshot.toggles.draw_cell_grid)
        cell_grid_renderer_.render(*m_window_, mouse_pos, 800.f);

    if (snapshot.toggles.draw_food_grid)
        food_manager_.draw_food_grid(mouse_pos);

    food_manager_.render(snapshot.food_data);
    render_protozoa(snapshot, font);

    m_window_->draw(world_border_renderer_);
}


void World::render_protozoa(const SimSnapshot& snapshot, Font* font)
{
    int cell_count = snapshot.stats.cell_count;

	render_springs(snapshot); // The springs are rendered first, so they appear behind the cells in the rendering order.

    // If simple mode is not enabled, the inner circles of the cells are rendered. 
    // The inner circles are scaled down by a factor defined in the graphical settings to create a visual distinction between the outer and inner parts of the cells.
    if (!toggles.simple_mode)
    {
        inner_radii_.resize(snapshot.render.radii.size());

        for (int i = 0; i < cell_count; ++i)
            inner_radii_[i] = snapshot.render.radii[i] / GraphicalSettings::cell_outline_thickness;

        inner_circle_renderer_.set_size(cell_count);
        inner_circle_renderer_.set_colors(snapshot.render.inner_colors);
        inner_circle_renderer_.set_positions_x(snapshot.render.positions_x);
        inner_circle_renderer_.set_positions_y(snapshot.render.positions_y);
        inner_circle_renderer_.set_radii(inner_radii_);

        inner_circle_renderer_.update();
        inner_circle_renderer_.render();
    }


	// The rendering of cells happens in two stages: first the outer circles are rendered, then the inner circles. 
    // The outer circles represent the cell's outer boundary, while the inner circles represent the cell's inner content. 
    // The rendering is done using a batch renderer for efficiency.
    outer_circle_renderer_.set_size(cell_count);
    outer_circle_renderer_.set_colors(snapshot.render.outer_colors);
    outer_circle_renderer_.set_positions_x(snapshot.render.positions_x);
    outer_circle_renderer_.set_positions_y(snapshot.render.positions_y);
    outer_circle_renderer_.set_radii(snapshot.render.radii);

	outer_circle_renderer_.update();
    outer_circle_renderer_.render();


	// If a protozoa is selected and debug mode is enabled, draw additional debug information for the selected protozoa.
    if (snapshot.protozoa_tracker.id != -1 && snapshot.toggles.debug_mode)
    {
        draw_protozoa_debug(snapshot, font);
    }
}


void World::render_springs(const SimSnapshot& snapshot)
{
	auto& positions_x = snapshot.render.positions_x;
	auto& positions_y = snapshot.render.positions_y;

	for (const std::pair <int, int>& spring_pair : snapshot.render.spring_connections)
	{
		const int cell_a_id = spring_pair.first;
		const int cell_b_id = spring_pair.second;

		const sf::Vector2f& pos1 = { positions_x[cell_a_id], positions_y[cell_a_id] };
		const sf::Vector2f& pos2 = { positions_x[cell_b_id], positions_y[cell_b_id] };

		if (toggles.debug_mode)
		{
			// the outline color should be that of cell a, the inline cololour should be that of cell b
            const sf::Color outline_color = {200, 50, 220};
            const sf::Color fill_color = { 200, 80, 220 };
			draw_thick_line(*m_window_, pos1, pos2, GraphicalSettings::spring_thickness,
				GraphicalSettings::spring_outline_thickness, fill_color, outline_color);
		}
	}
}


bool World::handle_mouse_click(const sf::Vector2f mouse_position)
{
	// When this function is called it checks every cell in the world to see if the mouse click is within the bounds of any cell. 
    // If it finds a cell that contains the mouse position, it sets that cell as the selected cell in the cell manager and returns true. 
    // If no cell is found, it returns false.
	// TODO: Use spatial grid to optimize this search instead of checking every cell.

	for (Cell* cell : cell_manager_.all_cells_)
	{
		Body* body = bodies_.at(cell->body_id_);

        float dist_sq = (mouse_position - body->position_).lengthSquared();

		bool in_bounds = dist_sq < cell->radius * cell->radius;
		if (in_bounds)
		{
			// We tell the cell manager which cell is selected, 
            // so it can be used in other parts of the program (like rendering debug info for the selected cell).
            cell_manager_.selected_cell = cell;
			return true;
		}
	}
    return false;
}

void World::keyboardEvents(const sf::Keyboard::Key& event_key_code)
{
    switch (event_key_code)
    {
	case sf::Keyboard::Key::G: // Toggle the drawing of the cell grid
        toggles.draw_cell_grid = !toggles.draw_cell_grid;
        break;

	case sf::Keyboard::Key::C: // Show collisions if not in debug mode else show connections between cells
        if (toggles.debug_mode)
            toggles.show_connections = !toggles.show_connections;
        else
            toggles.toggle_collisions = !toggles.toggle_collisions;
        break;

	case sf::Keyboard::Key::F: // Toggle the drawing of the food grid
        toggles.draw_food_grid = !toggles.draw_food_grid;
        break;

	case sf::Keyboard::Key::S: // only draw one of the two graphical representations of the cells (outer or inner circles)
        toggles.simple_mode = !toggles.simple_mode;
        break;

	case sf::Keyboard::Key::D: // Toggle debug mode
        toggles.debug_mode = !toggles.debug_mode;
        break;

	case sf::Keyboard::Key::T: // Toggle the tracking of statistics, in some cases can speed up simulation
        toggles.track_statistics = !toggles.track_statistics;
        break;

    case sf::Keyboard::Key::K: // This hides the graphical circles of the selected organism but still shows stats on its body
        if (toggles.debug_mode)
            toggles.skeleton_mode = !toggles.skeleton_mode;
        break;

	case sf::Keyboard::Key::B: // Toggle the drawing of bounding boxes around protozoa, only works in debug mode
        if (toggles.debug_mode)
            toggles.show_bounding_boxes = !toggles.show_bounding_boxes;
        break;

    default:
        break;
    }
}

const std::vector<float>& World::get_generation_distribution()
{
    distribution_.clear();

	for (const Cell* cell : cell_manager_.all_cells_)
		distribution_.push_back(static_cast<float>(cell->generation));

    return distribution_;
}

void World::update_spatial_renderers()
{
    cell_grid_renderer_.rebuild();
    food_manager_.update_food_grid_renderer();
}

void World::unload_render_data(SimSnapshot& snapshot)
{
    // we check if the rendering process has written any new data for us to use, we unload it onto the world before updating it
    
}

void World::fill_snapshot(SimSnapshot& snapshot)
{
    /* Data that goes to both the renderer and the ImGUI panels */

    snapshot.stats = get_statistics(); // simulation statistics
    snapshot.toggles = toggles;

	copy_render_data_to_snapshot(snapshot); // render data

    food_manager_.fill_data(snapshot.food_data);

    if (cell_manager_.selected_cell != nullptr)
    {
        protozoa_tracker_.update_primitive(cell_manager_.selected_cell, cell_manager_.all_cells_, cell_manager_.all_springs_, bodies_);
        snapshot.protozoa_tracker = protozoa_tracker_;
    }
	snapshot.selected_a_cell = cell_manager_.selected_cell != nullptr;

    copy_spatial_grids_to_snapshot(snapshot);
}


void World::copy_render_data_to_snapshot(SimSnapshot& snapshot)
{
    RenderData& render_data = snapshot.render;

    const int n = cell_manager_.get_protozoa_count();
    snapshot.stats.cell_count = n;

	// resizing the render data arrays to match the number of cells
    render_data.positions_x.resize(n);
    render_data.positions_y.resize(n);
    render_data.outer_colors.resize(n);
    render_data.inner_colors.resize(n);
    render_data.radii.resize(n);

	// efficiently copying the data from the internal render_data_ to the snapshot's render_data using memcpy
    std::memcpy(render_data.positions_x.data(), render_data_.positions_x.data(), n * sizeof(float));
    std::memcpy(render_data.positions_y.data(), render_data_.positions_y.data(), n * sizeof(float));
    std::memcpy(render_data.outer_colors.data(), render_data_.outer_colors.data(), n * sizeof(sf::Color));
    std::memcpy(render_data.inner_colors.data(), render_data_.inner_colors.data(), n * sizeof(sf::Color));
    std::memcpy(render_data.radii.data(), render_data_.radii.data(), n * sizeof(float));

    // now we handle springs, we can just store the indexes as then the renderer can read them from the positions container above
	const int spring_count = static_cast<int>(cell_manager_.all_springs_.size());
    render_data.spring_connections.resize(spring_count);
	for (Spring* spring : cell_manager_.all_springs_)
	{
		render_data.spring_connections[spring->id_] = { spring->cell_A_id, spring->cell_B_id };
	}
}

void World::copy_spatial_grids_to_snapshot(SimSnapshot& snapshot)
{
    // printing if there is a protozoa selected
    auto* cell_grid = get_spatial_grid();
    auto* food_grid = get_food_spatial_grid();

    snapshot.food_grid = get_grid_data(food_grid);
    snapshot.cell_grid = get_grid_data(cell_grid);

    if (toggles.track_spatial_grids)
    {
        calculate_spatial_grid_statistics(food_grid, snapshot.food_grid);
        calculate_spatial_grid_statistics(cell_grid, snapshot.cell_grid);
    }
}