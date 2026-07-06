#pragma once

#include "../world_settings.h"
#include "../../Utils/Graphics/SFML_Grid.h"
#include "../../Utils/utility_SFML.h"
#include "../collision_resolver/collision_resolver.h"
#include "../../Utils/Graphics/CircleBatchRenderer.h"
#include "../connection_renderer.h"
#include "../world_border.h"
#include "../../managers/cell_manager/cell_manager_settings.h"
#include "../../managers/food_manager/food_manager.h"
#include "../../simulation/context/sim_snapshot.h"

#include "../world_settings.h"


#include <SFML/Graphics/RenderWindow.hpp>
#include "../../Utils/Graphics/font_renderer.hpp"
#include "../..//simulation/settings/settings.h"


class WorldRenderer : public WorldSettings
{
	sf::RenderWindow* m_window_ = nullptr;
	FoodManager* food_manager_ = nullptr;
	CollisionResolver* collision_resolver_ = nullptr;

	// this is used as a frame of reference to see how fast cells are moving
	size_t _cells = static_cast<size_t>(CollisionResolver::cells_x / 4);
	SFML_Grid visual_grid_;

	// This is the circular world border that is drawn on the screen
	sf::VertexArray world_border_renderer_{};

	CircleBatchRenderer outer_circle_renderer_{};
	CircleBatchRenderer inner_circle_renderer_{};
	std::vector<float>  outer_radii_{};
	std::vector<sf::Vector2f>  outer_positions_{};
	std::vector<sf::Color>  colors_{};

	ConnectionRenderer connection_renderer_{};

	SpatialGridRenderer cell_grid_renderer_{ collision_resolver_->get_grid() };

public:
	// Constructor
	WorldRenderer(sf::RenderWindow* window, FoodManager* food_manager, CollisionResolver* collision_resolver, sf::FloatRect& bounds_rect, WorldBorder& circular_bounds)
		: 
		m_window_(window), food_manager_(food_manager), collision_resolver_(collision_resolver), visual_grid_(*m_window_, bounds_rect, _cells, 3, grid_color, grid_line_thickness),
		world_border_renderer_(make_circle(circular_bounds.bounds_radius, circular_bounds.center_))
	{
		init_circle_renderers();
	}

	void render(const SimSnapshot& snapshot, sf::Vector2f mouse_pos)
	{
		render_visual_grid(snapshot);

		if (snapshot.toggles.draw_food_grid)
			food_manager_->draw_food_grid(mouse_pos);

		if (snapshot.toggles.draw_cell_grid)
			cell_grid_renderer_.render(*m_window_, mouse_pos, 800.f);

		food_manager_->render(snapshot.food_data);
		render_protozoa(snapshot);

		m_window_->draw(world_border_renderer_);
	}

private:
	void init_circle_renderers()
	{
		float texture_radius = 120;
		outer_circle_renderer_.init(m_window_, texture_radius, CellManagerSettings::max_protozoa);
		inner_circle_renderer_.init(m_window_, texture_radius, CellManagerSettings::max_protozoa);

		const int max_entities = CellManagerSettings::max_protozoa + FoodManagerSettings::max_food;
	}

	void render_visual_grid(const SimSnapshot& snapshot)
	{
		float zoom = snapshot.sim_state.camera_zoom;
		float a = 1.f;
		if (zoom < start_fading_zoom)
		{
			a = (zoom - start_fading_zoom) / fade_zoom_dist;
			a = std::clamp(a, 0.f, 1.f);
		}
		visual_grid_.draw(a);
	}


	void render_protozoa(const SimSnapshot& snapshot)
	{
		int size = snapshot.render.positions.size();

		// The springs are rendered first, so they appear behind the cells in the rendering order.
		render_springs(snapshot);

		const float zoom = snapshot.sim_state.camera_zoom;
		bool simplify_colors = zoom < 0.05f;

		if (!simplify_colors)
		{
			inner_circle_renderer_.set_size(size);
			inner_circle_renderer_.set_colors(simplify_colors ? colors_ : snapshot.render.inner_colors);
			inner_circle_renderer_.set_positions(snapshot.render.positions);
			inner_circle_renderer_.set_radii(snapshot.render.radii);

			inner_circle_renderer_.update();
			inner_circle_renderer_.render();
		}

		// Cells are made from two circles, an inner circle and an outer circle. The Outer circle is only rendered if simple mode is disabled.
		if (!snapshot.toggles.simple_mode) // 0.05
		{
			outer_radii_.resize(size);
			outer_positions_.resize(size);

			for (int i = 0; i < size; ++i)
			{
				auto pos = snapshot.render.positions[i];
				auto vel = snapshot.render.velocities[i];
				const float base_radius = snapshot.render.radii[i];
				const float rad = base_radius * GraphicalSettings::cell_outline_thickness;
				const float margin = rad - base_radius; // available slack in the outline ring

				// Ease-out: strong response near the centre (t≈0), tapering to zero
				// additional movement as the offset nears the outer edge of the ring (t≈1).
				auto ease_out = [margin](float v) -> float
					{
						if (margin <= 0.f)
							return 0.f;

						const float sign = (v < 0.f) ? -1.f : 1.f;
						const float t = std::clamp(std::abs(v) / margin, 0.f, 1.f);
						const float eased = 1.f - (1.f - t) * (1.f - t); // 1 - (1-t)^2
						return sign * eased * margin;
					};

				const float scaled_x = ease_out(vel.x);
				const float scaled_y = ease_out(vel.y);

				outer_positions_[i] = pos - sf::Vector2f{ scaled_x, scaled_y };
				outer_radii_[i] = rad;
			}

			outer_circle_renderer_.set_size(size);
			outer_circle_renderer_.set_colors(snapshot.render.outer_colors);
			outer_circle_renderer_.set_positions(outer_positions_);
			outer_circle_renderer_.set_radii(outer_radii_);

			outer_circle_renderer_.update();
			outer_circle_renderer_.render();
		}

		// If a protozoa is selected and debug mode is enabled, draw additional debug information for the selected protozoa.
		if (snapshot.protozoa_tracker.is_active && snapshot.toggles.debug_mode)
		{
			draw_protozoa_debug(snapshot);
		}
	}


	void render_springs(const SimSnapshot& snapshot)
	{
		const float zoom = snapshot.sim_state.camera_zoom;
		if (zoom < 0.012f)
			return;

		bool no_curves = zoom < 0.08f;
		connection_renderer_.update(snapshot.render.spring_connections, no_curves);
		connection_renderer_.draw(*m_window_);
	}

	void draw_protozoa_debug(const SimSnapshot& snapshot)
	{
		const OrganismTracker& protozoa = snapshot.protozoa_tracker;

		if (protozoa.is_active == false)
			return;

		if (snapshot.toggles.skeleton_mode)
			draw_cell_outlines(protozoa);


		if (snapshot.toggles.show_bounding_boxes)
			draw_protozoa_bounding_box(protozoa.bounds, *m_window_);

		draw_cell_physical_information(snapshot);
	}


	void draw_cell_outlines(const OrganismTracker& protozoa)
	{
		sf::CircleShape circle_outline;
		circle_outline.setPointCount(30); // Reduce aliasing, set once
		int i = 0;
		for (const Cell& cell : protozoa.cells)
		{
			const Body& body = protozoa.bodies[i];

			const sf::Vector2f pos = body.position_;
			const float rad = cell.radius + GraphicalSettings::cell_outline_thickness;

			circle_outline.setRadius(rad);
			circle_outline.setFillColor({ 0, 0, 0 });
			circle_outline.setPosition(pos - sf::Vector2f{ rad, rad });
			m_window_->draw(circle_outline);

			circle_outline.setFillColor({ 255, 0, 255 });
			circle_outline.setRadius(rad / 3);
			circle_outline.setPosition(pos - sf::Vector2f{ rad / 3, rad / 3 });
			m_window_->draw(circle_outline);

			i++;
		}
	}


	void draw_cell_physical_information(const SimSnapshot& snapshot) const
	{
		const OrganismTracker& protozoa = snapshot.protozoa_tracker;

		// for each cell we draw its bounding box
		int i = 0;
		for (const Cell& cell : protozoa.cells)
		{
			const sf::Vector2f& pos = protozoa.bodies[i].position_;
			const sf::Vector2f& vel = protozoa.bodies[i].velocity_;
			const float speed = vel.length();
			const float rad = cell.radius;

			// rendering the bounding boxes
			const sf::FloatRect rect = { {pos.x - rad, pos.y - rad}, {rad * 2, rad * 2} };
			draw_protozoa_bounding_box(rect, *m_window_);

			// drawing the direction of the cell
			const float arrow_length = std::min(rad * 4, speed * rad);
			draw_direction(*m_window_, pos, vel, arrow_length, 6, 10,
				{ 200, 220, 200 }, { 190, 200, 190 });

			// drawing cell stats
			const auto top_left = rect.position;
			//const auto spacing = font->get_text_size("0").y;
			//const sf::Vector2f offset = { 0, spacing };
			//font->draw(top_left, "id: " + std::to_string(cell.body_id_), false);

			i++;
		}
	}
};