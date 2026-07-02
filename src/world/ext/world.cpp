#include "../world.h"

#include <algorithm>
#include "../../Utils/utility_SFML.h"
#include "../../Utils/Graphics/CircleBatchRenderer.h"
#include "../../entities/cell/cell_genome.h"
#include "simulation/settings/settings.h"

thread_local FixedSpan<uint32_t> World::tl_nearby_ids{25};
thread_local FixedSpan<obj_idx> World::tl_nearby_food{25};



World::World(sf::RenderWindow* window) : m_window_(window)
{
    food_manager_.init();

    render_data_.reserve(static_cast<int>(max_entities));
}


void World::render(const SimSnapshot& snapshot, Font* font, const sf::Vector2f mouse_pos)
{
	world_renderer_.render(snapshot, font, mouse_pos);
}


bool World::handle_mouse_click(const sf::Vector2f mouse_position)
{
    return cell_manager_.find_cell_at_point(mouse_position, true) != nullptr;
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


void World::update_spatial_renderers()
{
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
	cell_manager_.fill_snapshot(snapshot); // protozoa data

    copy_spatial_grids_to_snapshot(snapshot);
}


void World::copy_render_data_to_snapshot(SimSnapshot& snapshot)
{
    RenderData& render_data = snapshot.render;

    const int n = cell_manager_.get_cell_count();
    snapshot.stats.cell_count = n;

	// resizing the render data arrays to match the number of cells
    render_data.positions.resize(n);
	render_data.velocities.resize(n);
    render_data.outer_colors.resize(n);
    render_data.inner_colors.resize(n);
    render_data.radii.resize(n);

	// efficiently copying the data from the internal render_data_ to the snapshot's render_data using memcpy
    std::memcpy(render_data.positions.data(), render_data_.positions.data(), n * sizeof(sf::Vector2f));
    std::memcpy(render_data.velocities.data(), render_data_.velocities.data(), n * sizeof(sf::Vector2f));
    std::memcpy(render_data.outer_colors.data(), render_data_.outer_colors.data(), n * sizeof(sf::Color));
    std::memcpy(render_data.inner_colors.data(), render_data_.inner_colors.data(), n * sizeof(sf::Color));
    std::memcpy(render_data.radii.data(), render_data_.radii.data(), n * sizeof(float));

    render_data.body_debug_snapshot = FillSnapshot<Body>(dbg_bodies_, "Body", sf::Color::White);
    render_data.food_debug_snapshot = FillSnapshot<Food>(dbg_food_, "Food", sf::Color::White);
    render_data.spring_debug_snapshot = FillSnapshot<Spring>(dbg_springs_, "Spring", sf::Color::White);
	render_data.cell_debug_snapshot = FillSnapshot<Cell>(dbg_cells_, "Cell", sf::Color::White);
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

int World::check_mouse_press(const OrganismTracker& protozoa, const sf::Vector2f mousePosition, const bool tolerance_check) const
{
    for (const Cell& cell : protozoa.cells)
    {
        const Body* body = bodies_.at(cell.body_id_);
        const float dist_sq = (body->position_ - mousePosition).lengthSquared();
        float tollarance_factor = 1.2f;
        const float rad = cell.radius * tollarance_factor;
        if (dist_sq < rad * rad)
        {
            return cell.body_id_;
        }
    }

    return -1;
}