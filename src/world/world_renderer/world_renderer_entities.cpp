#include "world_renderer.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include <entities/body.h>
#include <entities/cell/cell.h>
#include <managers/cell_manager/cell_manager_settings.h>
#include <managers/cell_manager/organism_tracker.h>

#include "../../Utils/utility_SFML.h" // draw_protozoa_bounding_box


namespace
{
	constexpr float inner_circle_min_zoom = 0.05f;   // below this the inner circle layer is skipped
	constexpr float spring_min_zoom = 0.012f;        // below this springs aren't drawn at all
	constexpr float spring_curve_min_zoom = 0.08f;   // below this springs are drawn straight (curves aren't visible)

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

	void utilise_circle_renderer(CircleBatchRenderer& circle_renderer,
		const std::vector<sf::Color>& colors, const std::vector<sf::Vector2f>& positions, const std::vector<float>& radii)
	{
		assert(positions.size() == colors.size() && radii.size() == colors.size());

		circle_renderer.set_size(colors.size());
		circle_renderer.set_colors(colors);
		circle_renderer.set_positions(positions);
		circle_renderer.set_radii(radii);

		circle_renderer.update();
		circle_renderer.render();
	}
}


// ─────────────────────────────────────────────────────────────────────────────
//  Protozoa
// ─────────────────────────────────────────────────────────────────────────────
void WorldRenderer::render_protozoa(const SimSnapshot& snapshot)
{
	const auto& rd = snapshot.render;
	const size_t count = rd.positions.size();
	const float zoom = snapshot.sim_stats.camera_zoom;

	assert(rd.velocities.size() == count && rd.radii.size() == count);
	assert(rd.inner_colors.size() == count && rd.outer_colors.size() == count);

	// The inner fill is skipped entirely when zoomed far out (cells are sub-pixel there).
	if (zoom >= inner_circle_min_zoom)
		utilise_circle_renderer(inner_circle_renderer_, rd.inner_colors, rd.positions, rd.radii);

	// The outer circle is the outline: larger than the body, and slid opposite to the
	// velocity so faster moving protozoa get a trailing edge.
	const float outline_scale = CellManagerSettings::cell_outline_thickness;
	outer_radii_.resize(count);
	outer_positions_.resize(count);

	for (size_t i = 0; i < count; ++i)
	{
		const float base_radius = rd.radii[i];
		const float outer_radius = base_radius * outline_scale;
		const float margin = outer_radius - base_radius; // available slack in the outline ring

		const sf::Vector2f vel = rd.velocities[i];
		outer_positions_[i] = rd.positions[i] - sf::Vector2f{ ease_out(margin, vel.x), ease_out(margin, vel.y) };
		outer_radii_[i] = outer_radius;
	}

	utilise_circle_renderer(outer_circle_renderer_, rd.outer_colors, outer_positions_, outer_radii_);

	// If a protozoa is selected and debug mode is enabled, draw additional debug information for it.
	if (snapshot.protozoa_tracker.is_active && snapshot.world_toggles.debug_mode)
		draw_protozoa_debug(snapshot);
}


// ─────────────────────────────────────────────────────────────────────────────
//  Springs
// ─────────────────────────────────────────────────────────────────────────────
void WorldRenderer::render_springs(const SimSnapshot& snapshot)
{
	// We dont render springs if the camera is zoomed out too far
	const float zoom = snapshot.sim_stats.camera_zoom;
	if (zoom < spring_min_zoom || snapshot.cell_toggles.show_only_newborns)
		return;

	const bool no_curves = zoom < spring_curve_min_zoom; // optimization, if you cant see the curves, dont render them
	connection_renderer_.update(snapshot.render.spring_connections, no_curves);
	connection_renderer_.draw(*m_window_);
}


// ─────────────────────────────────────────────────────────────────────────────
//  Selected-organism debug overlays
// ─────────────────────────────────────────────────────────────────────────────
void WorldRenderer::draw_protozoa_debug(const SimSnapshot& snapshot)
{
	/* When the user has selected a protozoa we can draw more specific information on and around it */
	const OrganismTracker& protozoa = snapshot.protozoa_tracker;
	if (!protozoa.is_active)
		return;

	if (snapshot.cell_toggles.show_bounding_boxes)
		draw_protozoa_bounding_box(protozoa.bounds, *m_window_);

	draw_newborn_connection_radius(protozoa);
}

void WorldRenderer::draw_newborn_connection_radius(const OrganismTracker& protozoa)
{
	/* the radii at which newborn cells can connect */
	sf::CircleShape circle_outline;
	circle_outline.setPointCount(30);
	circle_outline.setFillColor({ 0, 0, 0, 0 });
	circle_outline.setOutlineColor({ 255, 255, 255, 100 });
	circle_outline.setOutlineThickness(10.f);

	// Index by position: cells[i] and bodies[i] are parallel arrays, so `i` must advance
	// for every cell, including the ones skipped below.
	for (int i = 0; i < protozoa.cell_count; ++i)
	{
		const Cell& cell = protozoa.cells[i];
		if (cell.internal_clock_ >= CellManagerSettings::infant_time)
			continue;

		const sf::Vector2f pos = protozoa.bodies[i].position_;
		const float rad = cell.newborn_search_radius;

		circle_outline.setRadius(rad);
		circle_outline.setPosition(pos - sf::Vector2f{ rad, rad });

		m_window_->draw(circle_outline);
	}
}