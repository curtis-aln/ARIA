#pragma once

#include <SFML/Graphics/RenderWindow.hpp>

#include "../world_border.h"
#include "../world_settings.h"
#include "connection_renderer.h"

#include "../../Utils/Graphics/CircleBatchRenderer.h"
#include "../../Utils/Graphics/SFML_Grid.h"

#include "../../Utils/utility_SFML.h"


#include "../../managers/cell_manager/cell_manager_settings.h"

#include "../../simulation/context/sim_snapshot.h"

#include "../../Utils/Graphics/pheromone_grid.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <entities/body.h>
#include <entities/cell/cell.h>
#include <managers/cell_manager/organism_tracker.h>
#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>
#include <Utils/spatial_grid/simple_spatial_grid.h>
#include <Utils/spatial_grid/spatial_grid_renderer.h>
#include <vector>


inline static constexpr int cells_div_value = 4; // cells_x / cells_div_value is the number of cells in the visual grid
inline static constexpr float world_border_thickness = 68.f; // thickness of the circular world border
inline static const sf::Color border_color = { 170, 200, 255, 100 }; // color of the circular world border
inline static constexpr int border_point_count = 256;
inline static constexpr float spatial_grid_query_radius = 500.f;


// Ease - out: strong response near the centre(t≈0), tapering to zero
// additional movement as the offset nears the outer edge of the ring (t≈1).
inline float ease_out(float margin, float v)
{
	if (margin <= 0.f)
		return 0.f;

	const float sign = (v < 0.f) ? -1.f : 1.f;
	const float t = std::clamp(std::abs(v) / margin, 0.f, 1.f);
	const float eased = 1.f - (1.f - t) * (1.f - t); // 1 - (1-t)^2
	return sign * eased * margin;
}

inline void utilise_circle_renderer(CircleBatchRenderer& circle_renderer,
	const std::vector<sf::Color>& colors, const std::vector<sf::Vector2f>& positions, const std::vector<float>& radii)
{
	circle_renderer.set_size(colors.size());
	circle_renderer.set_colors(colors);
	circle_renderer.set_positions(positions);
	circle_renderer.set_radii(radii);

	circle_renderer.update();
	circle_renderer.render();
}

// Returns the number of cells per side needed to keep each cell approximately
// `target_cell_size` wide as `world_size` changes, rounded up to the next
// power of two (required for Morton indexing in SimpleSpatialGrid).
inline uint32_t compute_grid_cell_count(float world_size, float target_cell_size)
{
	target_cell_size = std::max(target_cell_size, 1.f); // guard div-by-zero / negative sizes

	const uint32_t raw_cells = static_cast<uint32_t>(std::ceil(world_size / target_cell_size));
	return std::bit_ceil(std::max(raw_cells, 1u)); // next power of 2, min 1
}

/* This class renders pretty much the entire simulation, including the world, cells, and other entities. */
class WorldRenderer : public WorldSettings
{
	sf::RenderWindow* m_window_ = nullptr;

	// this is used as a frame of reference to see how fast cells are moving
	size_t cells_along_axis_ = compute_grid_cell_count(WorldSettings::bounds_radius, WorldSettings::target_visual_cell_size);
	SFML_Grid visual_grid_;

	// This is the circular world border that is drawn on the screen
	sf::CircleShape world_border_renderer_{};

	// Rendering the Cells, Food, and Cell matter
	CircleBatchRenderer outer_circle_renderer_{};
	CircleBatchRenderer inner_circle_renderer_{};
	std::vector<float>  outer_radii_{};
	std::vector<sf::Vector2f>  outer_positions_{};
	std::vector<sf::Color>  colors_{};

	// rendering the springs
	ConnectionRenderer connection_renderer_{};

	// rendering the spatial grids
	SpatialGridRenderer collision_grid_renderer_;
	SpatialGridRenderer food_grid_renderer_;
	SpatialGridRenderer newborn_grid_renderer_;

	// rendering the pheromones
	sf::Texture heat_texture{};
	sf::Sprite pheromone_sprite_{ heat_texture };


public:
	// Constructor
	WorldRenderer(
		sf::RenderWindow* window,
		SimpleSpatialGrid* collision_grid,
		SimpleSpatialGrid* food_grid,
		SimpleSpatialGrid* newborn_grid,
		PheromoneGrid* food_pheromone_grid,
		sf::FloatRect& bounds_rect,
		WorldBorder& circular_bounds)
		:
		m_window_(window),
		visual_grid_(*m_window_, bounds_rect, cells_along_axis_, 3, grid_color, grid_line_thickness),
		collision_grid_renderer_(collision_grid),
		food_grid_renderer_(food_grid),
		newborn_grid_renderer_(newborn_grid)
	{
		init_circle_renderers();
		init_world_border_renderer(circular_bounds);
	}

	void render(const SimSnapshot& snapshot, const sf::Vector2f mouse_pos)
	{
		render_pheromone_grid(snapshot);              // renders the pheromones that are left behind by the cells

		render_visual_grid(snapshot);              // renders the faint grid in the background of the simulation
		render_spatial_grids(snapshot, mouse_pos); // renders the spatial grids for food, collision, and newborn cells if enabled
		render_springs(snapshot);                  // renders the springs that connect the cells together
		render_protozoa(snapshot);                 // renders the protozoa (cells) and their springs
		render_influence_radii(snapshot);          // renders a circle around the mouse to show its influence radius when adding or removing entities

		render_world_border();                     // renders the circular world border
	}

private:
	void render_world_border()
	{
		m_window_->draw(world_border_renderer_);  // the glowing border ring, now sitting on a solid backdrop
	}

	void render_pheromone_grid(const SimSnapshot& snapshot)
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

	void render_influence_radii(const SimSnapshot& snapshot)
	{
		// renders a circle around the mouse to show its influence radius when adding or removing entities
		if (!snapshot.world_toggles.show_influence_radius)
			return;

		sf::Vector2f mouse_pos = { snapshot.sim_stats.mouse_pos_x, snapshot.sim_stats.mouse_pos_y };
		float influence_radius = snapshot.world_stats.mouse_radius;
		float outline_thickness = 2.f + influence_radius / 150.f; // scales with the zoom out of the camera
		const sf::Color influence_color = { 200, 215, 255, 100 };

		sf::CircleShape influence_circle;
		influence_circle.setPointCount(90);
		influence_circle.setRadius(influence_radius);
		influence_circle.setFillColor(sf::Color(0, 0, 0, 0));
		influence_circle.setOutlineColor(influence_color);
		influence_circle.setOutlineThickness(outline_thickness);
		influence_circle.setPosition(mouse_pos - sf::Vector2f(influence_radius, influence_radius));

		m_window_->draw(influence_circle);
	}

	void init_circle_renderers()
	{
		/* Initialize the circle renderers for the outer and inner circles. */
		constexpr float texture_radius = 120.f;
		outer_circle_renderer_.init(m_window_, texture_radius, CellManagerSettings::max_cells);
		inner_circle_renderer_.init(m_window_, texture_radius, CellManagerSettings::max_cells);
	}

	void init_world_border_renderer(const WorldBorder& circular_bounds)
	{
		world_border_renderer_.setRadius(circular_bounds.bounds_radius);
		world_border_renderer_.setFillColor(sf::Color(0, 0, 0, 0));
		world_border_renderer_.setPointCount(border_point_count);
		world_border_renderer_.setOutlineColor(border_color);
		world_border_renderer_.setOutlineThickness(world_border_thickness);
	}

	void render_visual_grid(const SimSnapshot& snapshot)
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

	void render_spatial_grids(const SimSnapshot& snapshot, const sf::Vector2f mouse_pos)
	{
		if (snapshot.world_toggles.draw_food_grid) // renders the food grid if the toggle is enabled
			food_grid_renderer_.render(*m_window_, mouse_pos, spatial_grid_query_radius);

		if (snapshot.world_toggles.draw_collision_grid) // renders the collision grid if the toggle is enabled
			collision_grid_renderer_.render(*m_window_, mouse_pos, spatial_grid_query_radius);

		if (snapshot.cell_toggles.show_newborn_grid) // remders the newborn grid if the toggle is enabled
			newborn_grid_renderer_.render(*m_window_, mouse_pos, spatial_grid_query_radius);
	}


	void render_protozoa(const SimSnapshot& snapshot)
	{
		// Data Fetching
		const size_t container_size = snapshot.render.positions.size();
		const float zoom = snapshot.sim_stats.camera_zoom;

		// This determines if the colors of the protozoa should be simplified based on the zoom level
		if (bool simplify_colors = zoom < 0.05f; !simplify_colors)
			utilise_circle_renderer(inner_circle_renderer_, simplify_colors ? colors_ : snapshot.render.inner_colors,
				snapshot.render.positions, snapshot.render.radii);

		// This is the outer circle renderer, which is used to render the outline of the protozoa. The outline is scaled based on the velocity of the protozoa, so that faster moving protozoa have a larger outline.
		outer_radii_.resize(container_size);
		outer_positions_.resize(container_size);

		for (size_t i = 0; i < container_size; ++i)
		{
			auto pos = snapshot.render.positions[i];
			auto vel = snapshot.render.velocities[i];
			const float base_radius = snapshot.render.radii[i];
			const float rad = base_radius * CellManagerSettings::cell_outline_thickness;
			const float margin = rad - base_radius; // available slack in the outline ring

			const float scaled_x = ease_out(margin, vel.x);
			const float scaled_y = ease_out(margin, vel.y);

			outer_positions_[i] = pos - sf::Vector2f{ scaled_x, scaled_y };
			outer_radii_[i] = rad;
		}

		// This draws the outer circle renderer, which is used to render the outline of the protozoa. The outline is scaled based on the velocity of the protozoa, so that faster moving protozoa have a larger outline.
		utilise_circle_renderer(outer_circle_renderer_, snapshot.render.outer_colors, outer_positions_, outer_radii_);

		// If a protozoa is selected and debug mode is enabled, draw additional debug information for the selected protozoa.
		if (snapshot.protozoa_tracker.is_active && snapshot.world_toggles.debug_mode)
			draw_protozoa_debug(snapshot);
	}


	void render_springs(const SimSnapshot& snapshot)
	{
		// We dont render springs if the camera is zoomed out too far
		const float zoom = snapshot.sim_stats.camera_zoom;
		if (zoom < 0.012f || snapshot.cell_toggles.show_only_newborns)
			return;

		bool no_curves = zoom < 0.08f; // optimization, if you cant see the curves, dont render them
		connection_renderer_.update(snapshot.render.spring_connections, no_curves);
		connection_renderer_.draw(*m_window_);
	}

	void draw_protozoa_debug(const SimSnapshot& snapshot)
	{
		/* When the user has selected a protozoa we can draw more specific information on and around it */
		const OrganismTracker& protozoa = snapshot.protozoa_tracker;
		if (protozoa.is_active == false)
			return;

		if (snapshot.cell_toggles.show_bounding_boxes)
			draw_protozoa_bounding_box(protozoa.bounds, *m_window_);

		draw_newborn_connection_radius(protozoa);
	}

	void draw_newborn_connection_radius(const OrganismTracker& protozoa)
	{
		/* the radii at which newborn cells can connect */
		sf::CircleShape circle_outline;
		circle_outline.setPointCount(30);
		circle_outline.setFillColor({ 0, 0, 0, 0 });
		circle_outline.setOutlineColor({ 255, 255, 255, 100 });
		circle_outline.setOutlineThickness(10.f);

		int i = 0;
		for (const Cell& cell : protozoa.cells)
		{
			if (cell.internal_clock_ >= CellManagerSettings::infant_time)
				continue;

			const Body& body = protozoa.bodies[i];
			const sf::Vector2f pos = body.position_;
			const float rad = cell.newborn_search_radius;

			circle_outline.setRadius(rad);
			circle_outline.setPosition(pos - sf::Vector2f{ rad, rad });

			m_window_->draw(circle_outline);
			i++;
		}
	}
};
