#include "../world.h"

#include <algorithm>
#include "../../Utils/utility_SFML.h"
#include "../../Utils/Graphics/CircleBatchRenderer.h"
#include "../../entities/cell/cell_genome.h"
#include "simulation/settings/settings.h"

thread_local FixedSpan<uint32_t> World::tl_nearby_ids{100};
thread_local FixedSpan<obj_idx> World::tl_nearby_food{100};

World::World(sf::RenderWindow* window)
    : m_window_(window), world_border_renderer_(make_circle(world_circular_bounds_.bounds_radius, world_circular_bounds_.center_)),
    outer_circle_renderer_(window, tex_rad, CellManagerSettings::max_protozoa), inner_circle_renderer_(window, tex_rad, CellManagerSettings::max_protozoa)
{
    claim_buffer.reserve(FoodManagerSettings::max_food);

    init_food_jobs();
    init_collision_jobs();
    cell_manager_.init_protozoa_container();


    render_data_.reserve(static_cast<int>(max_circles));
    distribution_.reserve(max_circles);
    cell_pointers_.resize(max_circles);
    inner_radii_.resize(max_circles);
    cell_manager_.collision_resolutions.resize(max_circles);
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
    outer_circle_renderer_.set_colors(snapshot.render.outer_colors);
    outer_circle_renderer_.set_positions_x(snapshot.render.positions_x);
    outer_circle_renderer_.set_positions_y(snapshot.render.positions_y);
    outer_circle_renderer_.set_radii(snapshot.render.radii);

    outer_circle_renderer_.set_size(snapshot.stats.entity_count);
	outer_circle_renderer_.update();
    outer_circle_renderer_.render();

    if (!toggles.simple_mode)
    {
        for (int i = 0; i < snapshot.stats.entity_count; ++i)
            inner_radii_[i] = snapshot.render.radii[i] / GraphicalSettings::cell_outline_thickness;

        inner_circle_renderer_.set_colors(snapshot.render.inner_colors);
        inner_circle_renderer_.set_positions_x(snapshot.render.positions_x);
        inner_circle_renderer_.set_positions_y(snapshot.render.positions_y);
        inner_circle_renderer_.set_radii(inner_radii_);
        inner_circle_renderer_.set_size(snapshot.stats.entity_count);
        inner_circle_renderer_.update();
        inner_circle_renderer_.render();
    }

    if (snapshot.protozoa_tracker.id != -1 && snapshot.toggles.debug_mode)
    {
        draw_protozoa_debug(snapshot, font);
    }
}


bool World::handle_mouse_click(const sf::Vector2f mouse_position)
{
	for (Cell* cell : cell_manager_.all_cells_)
	{
        float dist_sq = (mouse_position - cell->get_pos()).lengthSquared();

		bool in_bounds = dist_sq < cell->radius * cell->radius;
		if (in_bounds)
		{
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
    case sf::Keyboard::Key::G:
        toggles.draw_cell_grid = !toggles.draw_cell_grid;
        break;

    case sf::Keyboard::Key::C:
        if (toggles.debug_mode)
            toggles.show_connections = !toggles.show_connections;
        else
            toggles.toggle_collisions = !toggles.toggle_collisions;
        break;

    case sf::Keyboard::Key::F:
        toggles.draw_food_grid = !toggles.draw_food_grid;
        break;

    case sf::Keyboard::Key::S:
        toggles.simple_mode = !toggles.simple_mode;
        break;

    case sf::Keyboard::Key::D:
        toggles.debug_mode = !toggles.debug_mode;
        break;

    case sf::Keyboard::Key::T:
        toggles.track_statistics = !toggles.track_statistics;
        break;

    case sf::Keyboard::Key::K:
        if (toggles.debug_mode)
            toggles.skeleton_mode = !toggles.skeleton_mode;
        break;

    case sf::Keyboard::Key::B:
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

SpatialGridData World::get_grid_data(SimpleSpatialGrid* grid)
{
	return SpatialGridData{
		grid->CellsX,
		grid->CellsY,
		grid->cell_max_capacity,
		grid->world_width,
		grid->world_height,
		grid->cell_width,
		grid->cell_height,
	};
}

void World::advanced_grid_data(SimpleSpatialGrid* grid, SpatialGridData& data)
{
	data.total = 0;
	data.max_in = 0;
	data.full = 0;
	data.empty = 0;
	for (size_t i = 0; i < grid->get_total_cells(); ++i)
	{
		const uint8_t cell_capacity = grid->cell_capacities[i];
		data.total += cell_capacity;
		data.max_in = std::max<int>(cell_capacity, data.max_in);
		if (cell_capacity == grid->cell_max_capacity)
			data.full++;
		if (cell_capacity == 0)
			data.empty++;
	}
	data.inv = data.total > 0 ? 1.f / static_cast<float>(data.total) : 0.f;
}

void World::fill_snapshot(SimSnapshot& snapshot)
{
    const int n = statistics_.entity_count;

    snapshot.render.positions_x.resize(n);
    snapshot.render.positions_y.resize(n);
    snapshot.render.outer_colors.resize(n);
    snapshot.render.inner_colors.resize(n);
    snapshot.render.radii.resize(n);

    std::memcpy(snapshot.render.positions_x.data(), render_data_.positions_x.data(), n * sizeof(float));
    std::memcpy(snapshot.render.positions_y.data(), render_data_.positions_y.data(), n * sizeof(float));
    std::memcpy(snapshot.render.outer_colors.data(), render_data_.outer_colors.data(), n * sizeof(sf::Color));
    std::memcpy(snapshot.render.inner_colors.data(), render_data_.inner_colors.data(), n * sizeof(sf::Color));
    std::memcpy(snapshot.render.radii.data(), render_data_.radii.data(), n * sizeof(float));
    snapshot.render.entity_count = n;

    snapshot.stats = get_statistics();
    snapshot.toggles = toggles;
	snapshot.stats.protozoa_count = cell_manager_.get_protozoa_count();
	snapshot.stats.food_count = food_manager_.get_food_vector().size();
	snapshot.stats.average_generation = cell_manager_.calculate_average_generation();
    snapshot.stats.average_lifetime = cell_manager_.average_lifetime_;
    snapshot.stats.longest_lived_ever = cell_manager_.longest_lived_ever_;

    food_manager_.fill_data(snapshot.food_data);

    //protozoa_tracker_.update_statistics(selected_protozoa_, get_food_spatial_grid(), get_spatial_grid(), food_manager_.get_food_vector(), render_data_);
	snapshot.protozoa_tracker = protozoa_tracker_;
	if (cell_manager_.selected_cell != nullptr)
    {
		snapshot.selected_a_cell = true;

    }

    // printing if there is a protozoa selected
    snapshot.food_grid = get_grid_data(get_food_spatial_grid());
    snapshot.cell_grid = get_grid_data(get_spatial_grid());

	if (toggles.track_spatial_grids)
	{
		advanced_grid_data(get_food_spatial_grid(), snapshot.food_grid);
        advanced_grid_data(get_spatial_grid(), snapshot.cell_grid);
	}
}
