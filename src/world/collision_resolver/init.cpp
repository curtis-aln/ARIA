#include "collision_resolver.h"

void CollisionResolver::init_collision_jobs()
{
	for (int t = 0; t < thread_count_; ++t)
	{
		collision_jobs_.emplace_back([this, t] {
			drain(next_work_, (uint32_t)active_cells_.size(), batch_size_, [&](uint32_t i) {
				primitive_detect_collisions_for_grid_cell(active_cells_[i], collision_indexes_[t]);
				});
			});

		quick_collision_jobs_.emplace_back([this, t] {
			drain(next_work_, (uint32_t)current_total_cells_, 64, [&](uint32_t k) {
				const BodyId id = collision_bodies_->occupied_list[k];
				Body* obj = collision_bodies_->at(id);

				for (uint8_t j = 0; j < obj->nearby_ids_size_; ++j)
				{
					const BodyId idx = obj->nearby_ids_[j];
					if (idx <= obj->id_ || idx >= (BodyId)collision_bodies_->raw_object_store_.size())
						continue;

					Body* other = collision_bodies_->at(idx);
					if (!other || !other->active || other->get_mass() <= 0.f)
						continue;

					body2bodycollisiondetection(obj, other, collision_indexes_[t]);
				}
				});
			});
	}

	collision_thread_pool_.set_jobs(collision_jobs_);
	quick_collision_thread_pool_.set_jobs(quick_collision_jobs_);
}

void CollisionResolver::prepare_collision_detection()
{
	const uint32_t total_cells = spatial_grid_.CellsX * spatial_grid_.CellsY;

	active_cells_.clear();   // keeps capacity, so no realloc after the first frame
	for (uint32_t i = 0; i < total_cells; ++i)
		if (spatial_grid_.cell_capacities[i] != 0)
			active_cells_.push_back(i);

	const uint32_t active = static_cast<uint32_t>(active_cells_.size());
	const uint32_t threads = std::max<uint32_t>(1, std::min<uint32_t>(thread_count_, active));
	batch_size_ = std::clamp<uint32_t>(active / (threads * 8), 1u, 64u);
	next_work_.store(0, std::memory_order_relaxed);   // was next_cell_
}