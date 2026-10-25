#pragma once

#include <cstdint>
#include <vector>

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>
#include <Utils/spatial_grid/simple_spatial_grid.h>
#include <Utils/spatial_grid/spatial_grid_renderer.h>

#include "../world_border.h"
#include "../world_settings.h"
#include "connection_renderer.h"

#include "../../Utils/Graphics/CircleBatchRenderer.h"
#include "../../Utils/Graphics/SFML_Grid.h"
#include "../../Utils/Graphics/pheromone_grid.h"

#include "../../simulation/context/sim_snapshot.h"


inline constexpr int cells_div_value = 4; // cells_x / cells_div_value is the number of cells in the visual grid

// Returns the number of cells per side needed to keep each cell approximately
// `target_cell_size` wide as `world_size` changes, rounded up to the next
// power of two (required for Morton indexing in SimpleSpatialGrid).
uint32_t compute_grid_cell_count(float world_size, float target_cell_size);


/* This class renders pretty much the entire simulation, including the world, cells, and other entities.
 *
 * world_renderer.cpp          - construction, world border, background grid, spatial grids, pheromones, mouse radius
 * world_renderer_entities.cpp - protozoa circles, springs, selected-organism debug overlays
 */
class WorldRenderer : public WorldSettings
{
	sf::RenderWindow* m_window_ = nullptr;

	// this is used as a frame of reference to see how fast cells are moving
	size_t cells_along_axis_;
	SFML_Grid visual_grid_;

	// Rendering the Cells, Food, and Cell matter
	CircleBatchRenderer outer_circle_renderer_{};
	CircleBatchRenderer inner_circle_renderer_{};
	std::vector<float>         outer_radii_{};
	std::vector<sf::Vector2f>  outer_positions_{};

	// rendering the springs
	ConnectionRenderer connection_renderer_{};

	// rendering the spatial grids
	SpatialGridRenderer collision_grid_renderer_;
	SpatialGridRenderer food_grid_renderer_;
	SpatialGridRenderer newborn_grid_renderer_;

	// rendering the pheromones
	sf::Texture heat_texture{};
	sf::Sprite pheromone_sprite_{ heat_texture };

	// the circle drawn around the mouse when adding / removing / attracting / repelling.
	// Kept as a member so the vertex array isn't rebuilt every frame.
	sf::CircleShape influence_circle_{};
	float           influence_radius_ = -1.f; // radius the circle's geometry was last built for

public:
	// This is the circular world border that is drawn on the screen
	sf::CircleShape world_border_renderer_{};

	WorldRenderer(
		sf::RenderWindow* window,
		SimpleSpatialGrid* collision_grid,
		SimpleSpatialGrid* food_grid,
		SimpleSpatialGrid* newborn_grid,
		PheromoneGrid* food_pheromone_grid, // unused: pheromones are drawn from the snapshot texture
		sf::FloatRect& bounds_rect,
		WorldBorder& circular_bounds);

	void render(const SimSnapshot& snapshot, const sf::Vector2f mouse_pos);

private:
	// ── world_renderer.cpp ────────────────────────────────────────────────
	void init_circle_renderers();
	void init_world_border_renderer(const WorldBorder& circular_bounds);
	void init_influence_renderer();

	void render_world_border();
	void render_pheromone_grid(const SimSnapshot& snapshot);
	void render_visual_grid(const SimSnapshot& snapshot);
	void render_spatial_grids(const SimSnapshot& snapshot, const sf::Vector2f mouse_pos);
	void render_influence_radii(const SimSnapshot& snapshot);

	// ── world_renderer_entities.cpp ───────────────────────────────────────
	void render_protozoa(const SimSnapshot& snapshot);
	void render_springs(const SimSnapshot& snapshot);
	void draw_protozoa_debug(const SimSnapshot& snapshot);
	void draw_newborn_connection_radius(const OrganismTracker& protozoa);
};