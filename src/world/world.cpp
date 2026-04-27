#include "world.h"

#include <algorithm>
#include "../Utils/utility_SFML.h"
#include "../Utils/Graphics/CircleBatchRenderer.h"
#include "../protozoa/genetics/CellGenome.h"

thread_local FixedSpan<uint32_t> World::tl_nearby_ids{100};
thread_local FixedSpan<obj_idx> World::tl_nearby_food{100};

World::World(sf::RenderWindow* window)
    : m_window_(window),
	world_border_renderer_(make_circle(world_circular_bounds_.radius, world_circular_bounds_.center)), ProtozoaManager(m_window_)
{
    claim_buffer.reserve(FoodSettings::max_food);

    init_food_jobs();
    init_collision_jobs();
    init_organisms();

    const size_t maximum_cells = max_protozoa * ProtozoaSettings::max_cells;

    render_data_.reserve(static_cast<int>(maximum_cells));
    distribution_.reserve(maximum_cells);
    cell_pointers_.resize(maximum_cells);
    inner_radii_.resize(maximum_cells);
    collision_resolutions.resize(maximum_cells);
}

void World::render(const SimSnapshot& snapshot, Font* font, const sf::Vector2f mouse_pos)
{
    if (snapshot.toggles.draw_cell_grid)
        cell_grid_renderer_.render(*m_window_, mouse_pos, 800.f);

    if (toggles.draw_food_grid)
        food_manager_.draw_food_grid(mouse_pos);

    food_manager_.render(snapshot);
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

    if (snapshot.protozoa.id != -1 && snapshot.toggles.debug_mode)
    {
        render_debug(&snapshot.protozoa, font, snapshot.toggles.skeleton_mode,
            snapshot.toggles.show_connections,
            snapshot.toggles.show_bounding_boxes);
    }
}

void World::init_organisms()
{
    for (int i = 0; i < max_protozoa; ++i)
        all_protozoa_.emplace({ i });

    for (int i = initial_protozoa; i < max_protozoa; ++i)
    {
        all_protozoa_.at(i)->kill();
        all_protozoa_.at(i)->soft_reset();
        all_protozoa_.remove(i);
    }

    for (Protozoa* protozoa : all_protozoa_)
        generate_protozoa(*protozoa, world_circular_bounds_);
}

bool World::handle_mouse_click(const sf::Vector2f mouse_position)
{
    for (Protozoa* protozoa : all_protozoa_)
    {
		sf::Rect<float> bounds = calc_protozoa_bounds(protozoa);
		bool in_bounds = bounds.contains(mouse_position);
        if (in_bounds)
        {
            selected_protozoa_ = protozoa;
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
    for (Protozoa* protozoa : all_protozoa_)
        for (const Cell& cell : protozoa->get_cells())
            distribution_.push_back(static_cast<float>(cell.generation));

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
    snapshot.render = get_render_data();
    snapshot.stats = get_statistics();
    snapshot.toggles = toggles;
	snapshot.stats.protozoa_count = get_protozoa_count();
	snapshot.stats.food_count = get_food_count();
	snapshot.stats.average_generation = calculate_average_generation();
    snapshot.stats.average_lifetime = average_lifetime_;
    snapshot.stats.longest_lived_ever = longest_lived_ever_;

    food_manager_.fill_data(snapshot.food_data);

    if (selected_protozoa_ != nullptr)
    {
        snapshot.protozoa = *selected_protozoa_;
		snapshot.selected_a_protozoa = true;
    }

    snapshot.food_grid = get_grid_data(get_food_spatial_grid());
    snapshot.cell_grid = get_grid_data(get_spatial_grid());

	if (toggles.track_spatial_grids)
	{
		advanced_grid_data(get_food_spatial_grid(), snapshot.food_grid);
        advanced_grid_data(get_spatial_grid(), snapshot.cell_grid);
	}
}

sf::Rect<float> World::calc_protozoa_bounds(Protozoa* protozoa)
{
    sf::Rect<float> bounds;
    for (const Cell& cell : protozoa->get_cells())
    {
	    bounds.position.x = std::min(cell.position_.x, bounds.position.x);
	    bounds.position.y = std::min(cell.position_.y, bounds.position.y);
	    if (cell.position_.x > bounds.position.x + bounds.size.x)
            bounds.size.x = cell.position_.x - bounds.position.x;
        if (cell.position_.y > bounds.position.y + bounds.size.y)
            bounds.size.y = cell.position_.y - bounds.position.y;
    }
    return bounds;
}