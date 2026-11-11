#include "collision_resolver.h"
#include "collision_vector.h"
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <entities/body/body.h>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>
#include <Utils/o_vec/o_vector.hpp>
#include <Utils/spatial_grid/fixed_span.h>

thread_local FixedSpan<uint32_t> CollisionResolver::tl_nearby_ids_{ nearby_ids_max };

CollisionResolver::CollisionResolver(sf::Rect<float>* bounds, o_vector<Body>* entities,
	unsigned int init_thread_count, unsigned int max_collisions_per_thread, unsigned int max_particles)
	:
	collision_bodies_(entities), thread_count_(init_thread_count),
	spatial_grid_(2 << grid_power, 2 << grid_power, cell_max_capacity, bounds->size.x, bounds->size.y),
	collision_thread_pool_(static_cast<int>(thread_count_)),
	add_to_grid_thread_pool_(static_cast<int>(thread_count_)),
	quick_collision_thread_pool_(static_cast<int>(thread_count_))
{

	init_collision_jobs();
	collision_indexes_.resize(thread_count_, CollisionVector(max_collisions_per_thread));

	collision_thread_pool_.set_jobs(collision_jobs_);  // once
}


void CollisionResolver::resolve_existing_detections()
{
	for (auto& collision_vector : collision_indexes_)
		collision_vector.clear();

	current_total_cells_ = collision_bodies_->occupied_count;
	if (current_total_cells_ == 0)
		return;   // nothing active — skip the pool entirely, jobs stay untouched

	ensure_quick_collision_jobs_built();
	quick_collision_thread_pool_.run_and_wait();
}


void CollisionResolver::handle_collision_resolutions()
{
	//debug_collision_duplicates(); // debugging

	for (CollisionVector& collision_vector : collision_indexes_)
	{
		resolve_collision_vector_collisions(collision_vector);
	}
}

void CollisionResolver::resolve_collision_vector_collisions(CollisionVector& collision_vector)
{
	const int size = collision_vector.size();
	if (size == 0)
		return;

	Body* particle_a = nullptr;
	int cached_id = -1;

	for (CollisionPair pair : collision_vector)
	{
		if (pair.index_a != cached_id)
		{
			particle_a = collision_bodies_->at(pair.index_a);
			cached_id = pair.index_a;
		}

		resolve_pair_collision(particle_a, collision_bodies_->at(pair.index_b));
	}
}


void CollisionResolver::resolve_pair_collision(Body* a, Body* b)
{
	const float inv_mass_a = a->inv_mass();
	const float inv_mass_b = b->inv_mass();
	const float inv_mass_sum = inv_mass_a + inv_mass_b;
	assert(a->mass_ > 0.f && b->mass_ > 0.f);   // both massless/recycled: nothing to resolve

	const float contact_dist = a->get_radius() + b->get_radius();
	const sf::Vector2f offset = a->get_pos() - b->get_pos();   // points B -> A
	const float dist_sq = offset.lengthSquared();
	if (dist_sq >= contact_dist * contact_dist) return;

	// One sqrt. Exactly coincident bodies get an arbitrary normal instead of being skipped.
	float dist = std::sqrt(dist_sq);
	sf::Vector2f normal{ 1.f, 0.f };
	if (dist > 1e-6f) normal = offset / dist; else dist = 0.f;

	// Positional correction: the lighter body moves further
	const float penetration = contact_dist - dist;
	const sf::Vector2f correction = normal * (penetration * correction_factor / inv_mass_sum);
	a->translate(correction * inv_mass_a);
	b->translate(-correction * inv_mass_b);

	// Velocity resolution: skip if already separating
	const float closing_speed = (a->get_vel() - b->get_vel()).dot(normal);   // < 0 when approaching
	if (closing_speed >= 0.f) return;

	// J = (1 + e) * |v_n| / (1/m_a + 1/m_b): this is the reduced mass again
	const float j = -(1.f + restitution) * closing_speed / inv_mass_sum;
	const sf::Vector2f impulse = normal * j;

	a->apply_impulse(impulse);    // dv_a = J / m_a
	b->apply_impulse(-impulse);    // dv_b = J / m_b, opposite direction

	a->impulse_ += j;              // equal and opposite, so both bodies took the same magnitude
	b->impulse_ += j;
}


void CollisionResolver::close_program()
{

}