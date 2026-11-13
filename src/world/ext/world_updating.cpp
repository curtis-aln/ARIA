#include "../world.h"
#include <algorithm>
#include <entities/body/body.h>
#include <entities/cell/cell.h>
#include <entities/cell/cell_genome.h>
#include <entities/food/food.h>
#include <iostream>
#include <ostream>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Mouse.hpp>
#include <simulation/context/sim_snapshot.h>
#include <utility>

// This function is called by the simulation thread to update the world state
// write_snapshot is written to so the renderer knows what to draw
void World::update(SimSnapshot& write_snapshot)
{
	// Sanity Check
	if (get_entity_count() != bodies_.size())
		std::cerr << "Warning: Entity count mismatch! bodies_.size() = " << bodies_.size() << ", get_entity_count() = " << get_entity_count() << std::endl;

	// pre-update preparations
	world_circular_bounds_.bounds_radius = WorldSettings::bounds_radius;


	// Update function
	if (should_tick_sim())
	{
		update_wall_params();
		auto& dist = cell_manager_.get_generation_distribution();
		for (int i = 0; i < tick_sim_multiplier; ++i)
		{
			tick_sim();
			push_history(write_snapshot, dist);
		}
	}

	// selected cell logic
	drag_selected_cell_logic();
	cell_manager_.update_protozoa_tracker();

	// We always update the position container, otherwise the simulation jitters when paused// We always update the position container, otherwise the simulation jitters when paused
	fill_snapshot(write_snapshot);
}

void World::push_history(SimSnapshot& write_snapshot, const std::vector<float>& dist)
{
	auto& cm_stats = write_snapshot.cell_manager_stats; // cell manager statistics
	auto& fm_stats = write_snapshot.food_manager_stats; // food manager statistics

	m_history_.push(
		cm_stats.cell_count,
		fm_stats.food_count,
		dist,
		cm_stats.average_mutation_rate,
		cm_stats.average_mutation_range,
		cm_stats.average_offspring_count,
		cm_stats.average_lifetime,
		cm_stats.average_cells_per_protozoa,
		cm_stats.average_tendon_count,
		cm_stats.average_energy,
		cm_stats.cell_matter_count,
		cm_stats.tendon_count);
}

void World::update_wall_params()
{
	m_wall_params_.center = world_circular_bounds_.center_;
	m_wall_params_.world_radius = world_circular_bounds_.bounds_radius;
	m_wall_params_.repulsion_start = std::max(0.f, m_wall_params_.world_radius - kRepulsionZoneWidth);
	m_wall_params_.repulsion_start_sq = m_wall_params_.repulsion_start * m_wall_params_.repulsion_start;
	m_wall_params_.spring_k = kRepulsionCoeff / kRepulsionZoneWidth;
	m_wall_params_.gravity_k = world_toggles.central_gravity ? kCentralGravityCoeff : 0.f;
}

bool World::should_tick_sim()
{
	/* This takes out some messy code from the main function, it just keeps the tick-by-tick update process working */
	bool should = world_toggles.m_tick_frame_time || !world_toggles.paused;

	if (world_toggles.m_tick_frame_time) // This code allows up to step frame by frame through the update loop
		world_toggles.m_tick_frame_time = false;

	return should;
}

void World::drag_selected_cell_logic()
{
	/* Drag the selected cell to the mouse world position */
	if (!dragging)
		return;

	sf::Vector2i window_pos = sf::Mouse::getPosition(*m_window_);
	sf::Vector2f world_pos = m_window_->mapPixelToCoords(window_pos);
	cell_manager_.drag_selected_cell_to_point(world_pos, cell_drag_strength);
}

void World::tick_sim()
{
	/* This function runs one update cycle of the simulation, can be stacked */
	iteration_clock_.tick();

	food_manager_.update(statistics_.iterations_);                              // updating the food in the world
	cell_manager_.update(statistics_.iterations_);       // updating the cells, springs, and cell matter in the world
	food_eat_resolver_.resolve(statistics_.iterations_); // updating the interation between cells and food

	update_entities(); // updating the positions of all bodies, and handling collisions between them

	// once the iteration has been completed, we update the statistics for the next frame
	if (!world_toggles.track_statistics)
		return;

	update_statistics();
	cell_manager_.update_100frame_stats(statistics_.iterations_);
}


// Splits `total` items evenly across `thread_count` threads and returns the
// [begin, end) range owned by `thread_index`. Returns {0, 0} if there's nothing to do.
static std::pair<int, int> compute_thread_range(int thread_index, int total, int thread_count)
{
	if (total == 0)
		return { 0, 0 };

	const int chunk_size = std::max(1, (total + thread_count - 1) / thread_count);
	const int begin = std::min(thread_index * chunk_size, total);
	const int end = std::min(begin + chunk_size, total);

	return { begin, end };
}

void World::bound_bodies_in_range(int begin, int end)
{
	for (int k = begin; k < end; ++k)
	{
		Body* body = bodies_.at(bodies_.occupied_list[k]);
		bound_body_to_world(body);
	}
}

void World::ensure_update_jobs_built()
{
	if (update_jobs_built_)
		return;

	updating_bodies_.clear();
	updating_bodies_.reserve(updating_threads);
	std::cout << "Building update jobs for " << updating_threads << " threads." << std::endl;

	for (int t = 0; t < (int)updating_threads; ++t)
	{
		updating_bodies_.emplace_back([this, t]
			{
				const auto [begin, end] = compute_thread_range(t, current_total_bodies_, (int)updating_threads);
				bound_bodies_in_range(begin, end);
			});
	}

	thread_pool_.set_jobs(updating_bodies_);   // only ever called once
	update_jobs_built_ = true;
}


void World::update_entities()
{
	bound_bodies();

	if (world_toggles.toggle_collisions)
		collision_resolver_.run(statistics_.iterations_);
}


void World::bound_bodies()
{
	current_total_bodies_ = bodies_.occupied_count;
	ensure_update_jobs_built();
	thread_pool_.run_and_wait();
}


void World::bound_body_to_world(Body* body)
{
	const WallParams& w = m_wall_params_;

	body->update_physics();

	const sf::Vector2f pos = body->get_pos();
	const float ox = pos.x - w.center.x;
	const float oy = pos.y - w.center.y;
	const float dist_sq = ox * ox + oy * oy;

	// Gravity + soft wall accumulated here, applied to the body once at the end.
	float fx = -ox * w.gravity_k;
	float fy = -oy * w.gravity_k;

	if (dist_sq > w.repulsion_start_sq)
	{
		const float dist = std::sqrt(dist_sq);   // > 0 here, since dist_sq > repulsion_start_sq >= 0
		const float inv_dist = 1.f / dist;
		const float nx = ox * inv_dist;               // outward unit normal
		const float ny = oy * inv_dist;
		const float limit = w.world_radius - body->get_radius() * 2.f;

		if (dist > limit)
		{
			// Hard wall: reflect velocity only if moving outward, then clamp onto the limit.
			const sf::Vector2f vel = body->get_vel();
			const float outward = vel.x * nx + vel.y * ny;
			if (outward > 0.f)
			{
				const float j = (1.f + kBounceCoeff) * outward;
				body->set_vel({ vel.x - nx * j, vel.y - ny * j });
			}
			body->set_pos({ w.center.x + nx * limit, w.center.y + ny * limit });
		}
		else
		{
			// Soft wall: force = kRepulsionCoeff * depth^2 * width, which simplifies to spring_k * penetration^2.
			const float pen = dist - w.repulsion_start;
			const float mag = w.spring_k * pen * pen;
			fx -= nx * mag;
			fy -= ny * mag;
		}
	}

	body->apply_force({ fx, fy });
	body->impulse_ = 0.f;
}

void World::debug_sanity_checks()
{
	for (const Cell* cell : cell_manager_.get_all_cells())
	{
		const Body* body = bodies_.at(cell->body_id_);

		if (!bodies_.is_obj_active(cell->body_id_))
		{
			std::cout << "Removing MissMatched cell with body_id: " << cell->body_id_ << std::endl;
			bodies_.remove(cell->body_id_);
		}
	}

	for (const Food* cell : food_manager_.get_food_vector())
	{
		const Body* body = bodies_.at(cell->body_id_);

		if (!bodies_.is_obj_active(cell->body_id_))
		{
			std::cout << "Removing MissMatched food with body_id: " << cell->body_id_ << std::endl;
			bodies_.remove(cell->body_id_);
		}
	}

	for (Body* body : bodies_)
	{
		// Finding out if the body is missing a cell or food
		if (!cell_manager_.has_cell_with_body_id(body->id_) && !food_manager_.has_food_with_body_id(body->id_))
		{
			std::cout << "MissMatched body with id: " << body->id_ << std::endl;
			//bodies_.remove(body->id_);
		}
	}
}

sf::FloatRect World::calulcate_visible_range()
{
	auto window_size = static_cast<sf::Vector2i>(m_window_->getSize());
	sf::Vector2f top_left = m_window_->mapPixelToCoords({ 0, 0 });
	sf::Vector2f bottom_right = m_window_->mapPixelToCoords(sf::Vector2i{ window_size.x, window_size.y });

	sf::FloatRect visible_bounds = {
		{ top_left.x, top_left.y },
		{ bottom_right.x - top_left.x, bottom_right.y - top_left.y } };

	// Adjust for cell maximim radius to ensure cells partially in view are included
	const float max_mass = CellGeneticConstraints::mass.max;
	const float max_rad = Body::mass_to_radius(max_mass);
	visible_bounds.position.x -= max_rad;
	visible_bounds.position.y -= max_rad;
	visible_bounds.size.x += max_rad * 2.f;
	visible_bounds.size.y += max_rad * 2.f;

	return visible_bounds;
}