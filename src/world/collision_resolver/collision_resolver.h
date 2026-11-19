#pragma once

/* Collision Resolver Class
This class handles collision detection and resolution between particles in a 2D simulation. 
It uses a spatial grid to efficiently detect potential collisions and resolves them based on their positions and velocities.
Modify the Settings struct to change the grid size, cell capacity, and collision resolution parameters.
*/

#include "../../Utils/o_vec/o_vector.hpp" // Containing the particles
#include "../../entities/body/body.h" // The particle class Themselves
#include "../../Utils/spatial_grid/simple_spatial_grid.h" // Collision Detection
#include "../../Utils/thread_pool.h" // Multithreadding

#include <functional>
#include <set>

#include "collision_vector.h" // To know what to resolve

inline static constexpr int collision_resolution_interval = 30; // how often to run full collision detection, in iterations


struct ResolutionSettings
{
	// 1, 2, 4, 8, 16, 32, 64, 128
	inline static uint32_t grid_power = 7; // 2^grid_power = number of cells in each dimension
	inline static const uint8_t cell_max_capacity = 12; // maximum number of particles per cell, must be less than 256

	inline static constexpr float correction_factor = 0.3f; // how much of the overlap is corrected each frame, 0.2 is a good value, 1.0 is too much and causes jittering
	inline static constexpr float restitution = .5f; // how much of the velocity is retained after a collision, 1.0 is perfectly elastic, 0.0 is perfectly inelastic
};

// The maximum number of nearby particles that can be detected for a given particle, 
// this is used to allocate the thread local buffer for nearby particles
inline static const int nearby_ids_max = ResolutionSettings::cell_max_capacity * 9;


// This class is resonsible for the updating and rendering of the particles in the simulation
class CollisionResolver : public ResolutionSettings
{
	int thread_count_{};

	std::vector<uint32_t>  active_cells_;
	uint32_t              batch_size_{ 1 };

	o_vector<Body>* collision_bodies_; // pointer to the particle vector, not owned by this class

	// The grid used for collision resolution
	SimpleSpatialGrid spatial_grid_;

	// for multithreadded collision resolution
	BarrierThreadPool collision_thread_pool_;
	BarrierThreadPool quick_collision_thread_pool_;

	// Pre-Creating the thread jobs for the collision detection and resolution
	std::vector<std::function<void()>> collision_jobs_;
	std::vector<std::function<void()>> quick_collision_jobs_;

	// This is used in the collision detection to collect all the nearby particles for a given cell
	static thread_local FixedSpan<uint32_t> tl_nearby_ids_;

	// This is used to store the collisions detected by each thread, each thread has its own collision vector to avoid contention
	std::vector<CollisionVector> collision_indexes_{};

	// ---------------------------
	int resolution_frame_ = 0;  // toggles 0/1 each frame

	int  current_total_cells_ = 0;

	std::atomic<uint32_t> next_work_{ 0 };

	template <class F>
	static void drain(std::atomic<uint32_t>& counter, uint32_t total, uint32_t batch, F&& fn)
	{
		for (;;)
		{
			const uint32_t s = counter.fetch_add(batch, std::memory_order_relaxed);
			if (s >= total) return;
			const uint32_t e = std::min(s + batch, total);
			for (uint32_t i = s; i < e; ++i) fn(i);
		}
	}

public:
	CollisionResolver(sf::Rect<float>* bounds, o_vector<Body>* entities, 
		unsigned int init_thread_count, unsigned int max_collisions_per_thread, unsigned int max_particles);

	void run(const int iterations)
	{
		if (iterations % collision_resolution_interval != 0)
		{
			resolve_existing_detections();
			handle_collision_resolutions();
			return;
		}
		
		add_particles_to_grid();
		run_collision_detection();
		handle_collision_resolutions();
	}


	void resolve_existing_detections();


	// This clears the grid and re-adds all particles to the grid, this is less efficient than update_particles_grid_indexes but is simpler and 
	// faster if the particles move a lot
	void add_particles_to_grid();

	// detects collisions for all particles in the grid, this is done in parallel using the thread pool, all collisions are stored in the collision_indexes vector, which is then used to resolve the collisions
	void run_collision_detection();

	// resolves all collisions in the collision_indexes vector, this is done in parallel using the thread pool
	void handle_collision_resolutions();

	// Closes The Threads safely
	void close_program();

	// Fetching Functions
	SimpleSpatialGrid* get_grid() { return &spatial_grid_; }
	const SimpleSpatialGrid* get_grid() const { return &spatial_grid_; }

private:
	// The collision jobs for the threads are pre-calculated so there is no overhead of creating them each frame
	void init_collision_jobs();            // once
	void prepare_collision_detection();    // every detection frame
	
	// Collision Detection Functions
	void primitive_detect_collisions_for_grid_cell(const int grid_cell_id, CollisionVector& collision_vector);
	void detect_collisions_for_grid_cell(const int grid_cell_id, FixedSpan<uint32_t>& nearby_ids, CollisionVector& collision_vector);
	void update_nearby_container(const int32_t neighbour_index_x, const int32_t neighbour_index_y, FixedSpan<uint32_t>& nearby_ids);
	void check_collisions_for_body(const BodyId protozoa_cell_index, const FixedSpan<uint32_t>& nearby_ids, CollisionVector& collision_vector, int check_count = -1);

	void body2bodycollisiondetection(const Body* protozoa_cell, const Body* other_cell, CollisionVector& collision_vector);

	// Collision Resolution Functions
	void resolve_collision_vector_collisions(CollisionVector& collision_vector);
	void resolve_pair_collision(Body* particle_a, Body* particle_b);
};