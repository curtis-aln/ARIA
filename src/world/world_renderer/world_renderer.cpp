#include "world_renderer.h"

#include <algorithm>
#include <bit>
#include <cmath>

#include <managers/cell_manager/cell_manager_settings.h>


namespace
{
	constexpr float world_border_thickness = 68.f;       // thickness of the circular world border
	constexpr int   border_point_count = 256;
	constexpr float spatial_grid_query_radius = 500.f;
	constexpr int   influence_point_count = 90;

	const sf::Color border_color = { 170, 200, 255, 100 };    // color of the circular world border
	const sf::Color influence_color = { 200, 215, 255, 100 }; // color of the mouse influence ring
}


uint32_t compute_grid_cell_count(float world_size, float target_cell_size)
{
	target_cell_size = std::max(target_cell_size, 1.f); // guard div-by-zero / negative sizes

	const uint32_t raw_cells = static_cast<uint32_t>(std::ceil(world_size / target_cell_size));
	return std::bit_ceil(std::max(raw_cells, 1u)); // next power of 2, min 1
}


// ─────────────────────────────────────────────────────────────────────────────
//  Construction
// ─────────────────────────────────────────────────────────────────────────────
WorldRenderer::WorldRenderer(
	sf::RenderWindow* window,
	SimpleSpatialGrid* collision_grid,
	SimpleSpatialGrid* food_grid,
	SimpleSpatialGrid* newborn_grid,
	PheromoneGrid* /*food_pheromone_grid*/,
	sf::FloatRect& bounds_rect,
	WorldBorder& circular_bounds)
	:
	m_window_(window),
	cells_along_axis_(compute_grid_cell_count(WorldSettings::bounds_radius, WorldSettings::target_visual_cell_size)),
	visual_grid_(*m_window_, bounds_rect, cells_along_axis_, 3, grid_color, grid_line_thickness),
	collision_grid_renderer_(collision_grid),
	food_grid_renderer_(food_grid),
	newborn_grid_renderer_(newborn_grid)
{
	init_circle_renderers();
	init_world_border_renderer(circular_bounds);
	init_influence_renderer();
}

void WorldRenderer::init_circle_renderers()
{
	/* Initialize the circle renderers for the outer and inner circles. */
	constexpr float texture_radius = 120.f;
	outer_circle_renderer_.init(m_window_, texture_radius, CellManagerSettings::max_cells);
	inner_circle_renderer_.init(m_window_, texture_radius, CellManagerSettings::max_cells);
}

void WorldRenderer::init_world_border_renderer(const WorldBorder& circular_bounds)
{
	world_border_renderer_.setRadius(circular_bounds.bounds_radius);
	world_border_renderer_.setFillColor(sf::Color(0, 0, 0, 0));
	world_border_renderer_.setPointCount(border_point_count);
	world_border_renderer_.setOutlineColor(border_color);
	world_border_renderer_.setOutlineThickness(world_border_thickness);
}

void WorldRenderer::init_influence_renderer()
{
	influence_circle_.setPointCount(influence_point_count);
	influence_circle_.setFillColor(sf::Color(0, 0, 0, 0));
	influence_circle_.setOutlineColor(influence_color);
}


// ─────────────────────────────────────────────────────────────────────────────
//  Per-frame entry point
// ─────────────────────────────────────────────────────────────────────────────
void WorldRenderer::render(const SimSnapshot& snapshot, const sf::Vector2f mouse_pos)
{
	render_pheromone_grid(snapshot);           // renders the pheromones that are left behind by the cells

	render_visual_grid(snapshot);              // renders the faint grid in the background of the simulation
	render_spatial_grids(snapshot, mouse_pos); // renders the spatial grids for food, collision, and newborn cells if enabled
	render_springs(snapshot);                  // renders the springs that connect the cells together
	render_protozoa(snapshot);                 // renders the protozoa (cells) and their debug overlays
	render_influence_radii(snapshot);          // renders a circle around the mouse to show its influence radius when adding or removing entities

	render_world_border();                     // renders the circular world border
}


// ─────────────────────────────────────────────────────────────────────────────
//  World, grids, pheromones, mouse
// ─────────────────────────────────────────────────────────────────────────────
void WorldRenderer::render_world_border()
{
	m_window_->draw(world_border_renderer_);  // the glowing border ring, now sitting on a solid backdrop
}

void WorldRenderer::render_pheromone_grid(const SimSnapshot& snapshot)
{
	if (!snapshot.food_toggles.render_pheromone_grid)
		return;

	pheromone_sprite_.setTexture(snapshot.render.pheromone_texture, true);
	const sf::Vector2f size = {
		static_cast<float>(snapshot.food_manager_stats.pheromone_grid_cells_x),
		static_cast<float>(snapshot.food_manager_stats.pheromone_grid_cells_y) };
	pheromone_sprite_.setScale(size);
	pheromone_sprite_.setPosition({ 0.f, 0.f });

	m_window_->draw(pheromone_sprite_, sf::RenderStates());
}

void WorldRenderer::render_visual_grid(const SimSnapshot& snapshot)
{
	// The visual grid is the faint grid that is drawn in the background of the simulation.
	// It is used to help visualize the scale of the simulation and to help with debugging.

	if (!snapshot.world_toggles.draw_background_grid)
		return;

	float zoom = snapshot.sim_stats.camera_zoom;
	float alpha_value = 1.f;
	if (zoom < start_fading_zoom)
	{
		alpha_value = (zoom - start_fading_zoom) / fade_zoom_dist;
		alpha_value = std::clamp(alpha_value, 0.f, 1.f);
	}
	visual_grid_.draw(alpha_value);
}

void WorldRenderer::render_spatial_grids(const SimSnapshot& snapshot, const sf::Vector2f mouse_pos)
{
	if (snapshot.world_toggles.draw_food_grid) // renders the food grid if the toggle is enabled
		food_grid_renderer_.render(*m_window_, mouse_pos, spatial_grid_query_radius);

	if (snapshot.world_toggles.draw_collision_grid) // renders the collision grid if the toggle is enabled
		collision_grid_renderer_.render(*m_window_, mouse_pos, spatial_grid_query_radius);

	if (snapshot.cell_toggles.show_newborn_grid) // remders the newborn grid if the toggle is enabled
		newborn_grid_renderer_.render(*m_window_, mouse_pos, spatial_grid_query_radius);
}

void WorldRenderer::render_influence_radii(const SimSnapshot& snapshot)
{
	// renders a circle around the mouse to show its influence radius when adding or removing entities
	if (!snapshot.world_toggles.show_influence_radius)
		return;

	const float radius = snapshot.world_stats.mouse_radius;

	// Only rebuild the 90-point vertex array when the radius actually changes.
	if (radius != influence_radius_)
	{
		influence_radius_ = radius;
		influence_circle_.setRadius(radius);
		influence_circle_.setOutlineThickness(2.f + radius / 150.f);
	}

	const sf::Vector2f mouse_pos = { snapshot.sim_stats.mouse_pos_x, snapshot.sim_stats.mouse_pos_y };
	influence_circle_.setPosition(mouse_pos - sf::Vector2f(radius, radius));

	m_window_->draw(influence_circle_);
}