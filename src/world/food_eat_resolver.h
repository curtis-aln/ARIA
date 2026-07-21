#pragma once

/*
This class handles the resolution of cels trying to eat food. it is very optimized and this is the process it follows
Firstly All the food are appended to a spatial grid, to allow us to access nearby food from the cell positions.

Then we create 16 or so threads Each With their own BiteResolutions Object

In the situation where a cell overlaps food, it will write the foods id to the bite resolution object and update its nutrients

At the end of the 16 threads, join them
We then single threadedly go through all 16 of these BiteResolutions and take the appropriate amount of nutrients from the food

Foodmanager will handle if the food gets removed
*/

#pragma once

#include <vector>
#include <assert.h>
#include <cstdint>
#include <iterator>
#include <Utils/spatial_grid/simple_spatial_grid.h>
#include <Utils/o_vec/o_vector.hpp>
#include <entities/food/food.h>
#include <entities/cell/cell.h>
#include "../Utils/thread_pool.h" // Multithreadding


struct alignas(64) BiteResolution
{
	struct Iterator
	{
		using iterator_category = std::forward_iterator_tag;
		using value_type = obj_idx;
		using difference_type = std::ptrdiff_t;
		using pointer = const obj_idx*;
		using reference = const obj_idx&;

		explicit Iterator(const obj_idx* ptr) : ptr_(ptr) {}

		reference operator*() const { return *ptr_; }

		Iterator& operator++() { ++ptr_; return *this; }
		Iterator operator++(int) { Iterator tmp = *this; ++ptr_; return tmp; }

		bool operator==(const Iterator& other) const { return ptr_ == other.ptr_; }
		bool operator!=(const Iterator& other) const { return ptr_ != other.ptr_; }

	private:
		const obj_idx* ptr_;
	};

	explicit BiteResolution(int reserve)
	{
		food_ids_.resize(reserve);
	}

	void add(obj_idx food_id)
	{
		assert(size_ < static_cast<int>(food_ids_.size())
			&& "BiteResolution capacity exceeded — increase reserve size.");

		food_ids_[size_++] = food_id;
	}

	void clear() { size_ = 0; }

	[[nodiscard]] int  size()     const { return size_; }
	[[nodiscard]] bool empty()    const { return size_ == 0; }
	[[nodiscard]] int  capacity() const { return static_cast<int>(food_ids_.size()); }

	[[nodiscard]] obj_idx operator[](int index) const
	{
		assert(index >= 0 && index < size_ && "BiteResolution index out of range.");
		return food_ids_[index];
	}

	Iterator begin() const { return Iterator(food_ids_.data()); }
	Iterator end()   const { return Iterator(food_ids_.data() + size_); }

private:
	std::vector<obj_idx> food_ids_{};
	int size_ = 0;
};

struct FoodResolutionSettings
{
	inline static uint32_t cells_x = (1u << 8); // for morton indexing, must be a power of 2
	inline static uint32_t cells_y = cells_x;     // square worlds
	inline static const uint32_t cell_max_capacity = 6; // maximum number of particles per cell, must be less than 256, but really shouldnt be any greater than 6
	inline static const int threads = 15;
};

inline static thread_local FixedSpan<uint32_t> tl_nearby_ids_{100};

class FoodEatResolver: private FoodResolutionSettings
{
	o_vector<Food>* food_vector_ = nullptr; // pointer to the food vector, not owned by this class
	o_vector<Body>* body_vector_ = nullptr; // pointer to the body vector, not owned by this class
	o_vector<Cell>* cell_vector_ = nullptr; // pointer to the cell vector, not owned by this class

	SimpleSpatialGrid spatial_grid_; // The grid used for food resolution

	sf::FloatRect world_bounds_; // The bounds of the world, used to determine if a cell is in bounds

	// Multithreadding
	std::vector<std::function<void()>> detection_jobs_;
	BarrierThreadPool thread_pool_{ threads };
	
	std::vector<BiteResolution> bite_resolutions_;

	int  current_total_cells_ = 0;
	bool detection_jobs_built_ = false;

public:
	FoodEatResolver(
		o_vector<Food>* food_vector, o_vector<Body>* body_vector, o_vector<Cell>* cell_vector, 
		unsigned thread_count, unsigned reserve_per_thread, sf::FloatRect world_bounds)
		: food_vector_(food_vector), body_vector_(body_vector), cell_vector_(cell_vector),
		spatial_grid_(cells_x, cells_y, cell_max_capacity, world_bounds.size.x, world_bounds.size.y), 
		world_bounds_(world_bounds)
	{
		init_bite_resolutions(reserve_per_thread);
	}

	void ensure_detection_jobs_built()
	{
		if (detection_jobs_built_)
			return;

		detection_jobs_.clear();
		detection_jobs_.reserve(threads);

		// For each of the threads
		for (int t = 0; t < (int)threads; ++t)
		{
			detection_jobs_.emplace_back([this, t] {
				const int total_cells = current_total_cells_;
				if (total_cells == 0)
					return;

				const int chunk = std::max(1, (total_cells + (int)threads - 1) / (int)threads);
				const int begin = t * chunk;
				if (begin >= total_cells)
					return;
				const int end = std::min(begin + chunk, total_cells);

				for (int k = begin; k < end; ++k)
				{
					const int cell_id = cell_vector_->occupied_list[k];
					detect_bite_for_cell(body_vector_->at(cell_id), bite_resolutions_[t]);
				}
				});
		}

		thread_pool_.set_jobs(detection_jobs_);   // only ever called this once
		detection_jobs_built_ = true;
	}

	void resolve()
	{
		add_food_to_grid();

		current_total_cells_ = cell_vector_->occupied_count;
		if (current_total_cells_ == 0)
			return;   // nothing to detect or resolve — skip the pool entirely

		ensure_detection_jobs_built();
		food_bite_detection();
		food_bite_resolution();
	}

	

	// Fetching Functions
	SimpleSpatialGrid& get_spatial_grid() { return spatial_grid_; }


private:
	void init_bite_resolutions(int reserve_per_thread)
	{
		bite_resolutions_.resize(threads, BiteResolution(reserve_per_thread));
	}


	void clear_bite_resolutions()
	{
		for (auto& br : bite_resolutions_)
			br.clear();
	}

	// This function will add all the food to the spatial grid, so that we can easily find nearby food for each cell
	void add_food_to_grid()
	{
		spatial_grid_.clear();
		for (Food* food : *food_vector_)
		{
			Body* body = body_vector_->at(food->body_id_);
			spatial_grid_.add_object(body->position_.x, body->position_.y, food->id_);
		}
	}

	// This function will detect if any cells are overlapping with food, and if so, it will add the food id to the bite resolution for that cell
	void food_bite_detection()
	{
		clear_bite_resolutions();
		thread_pool_.run_and_wait();
	}

	float detect_bite_for_cell(Body* body, BiteResolution& resolution)
	{
		tl_nearby_ids_.count = 0;
		spatial_grid_.find(body->position_.x, body->position_.y, &tl_nearby_ids_);

		float transfer = 0;
		for (int i = 0; i < tl_nearby_ids_.count; ++i)
		{
			obj_idx food_id = tl_nearby_ids_[i];
			Food* food = food_vector_->at(food_id);
			Body* food_body = body_vector_->at(food->body_id_);

			sf::Vector2f food_pos = food_body->position_;
			float food_radius = food_body->radius_;

			float dist_sq = (food_pos - body->position_).lengthSquared();
			float rel_rad = food_radius + body->radius_;
			float rel_rad_sq = rel_rad * rel_rad;

			if (dist_sq < rel_rad_sq + 1.f)
			{
				// The cell is in contact with the food
				transfer = CellSettings::bite_amount;
				resolution.add(food_id);
			}
		}
		return transfer;
	}
		
	// This function will resolve the bites, by taking the appropriate amount of nutrients from the food and adding it to the cell
	void food_bite_resolution()
	{
		for (BiteResolution& br : bite_resolutions_)
		{
			for (obj_idx food_id : br)
			{
				Food* food = food_vector_->at(food_id);
				float transfer = CellSettings::bite_amount;
				food->nutrients -= transfer;
			}
		}
	}
};