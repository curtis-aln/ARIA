#pragma once
#include <queue>
#include <SFML/System/Vector2.hpp>

#include "../settings.h"
#include "cell.h"
#include "spring.h"


// this is the organisms in the simulation, they are made up of cells which all act independently, attached by springs
// the protozoa class is responsible for connecting the springs and cells



// The Genome class handles anything that has to do with mutation and genetics
class Protozoa : ProtozoaSettings
{
public:
	int id = 0;
	bool active = true; // for o_vector.h

	// statistics informaiton
	

private:
	// components of the protozoa
	alignas (64) std::vector<Cell> m_cells_{};
	alignas (64) std::vector<Spring> m_springs_{};	

	
public:
	Protozoa(int id_ = 0);
	Protozoa(const Protozoa& other);
	Protozoa& operator=(const Protozoa& other);
	
	void mutate(const bool artificial_add_cell = false, const float artificial_mutatation_rate = 0.f, const float artificial_mutatation_range = 0.f);

	std::vector<Cell>& get_cells() { return m_cells_; }
	[[nodiscard]] const std::vector<Cell>& get_cells() const { return m_cells_; }
	std::vector<Spring>& get_springs() { return m_springs_; }
	[[nodiscard]] const std::vector<Spring>& get_springs() const { return m_springs_; }


	[[nodiscard]] unsigned get_frames_alive_avg() const
	{
		unsigned sum = 0;
		unsigned count = m_cells_.size();

		for (const Cell& cell : m_cells_)
		{
			sum += cell.frames_alive_;
		}
		return count > 0 ? sum / count : 0;
	}

	[[nodiscard]] size_t stomach_reproduce_thresh() const { return m_cells_.size(); }
	[[nodiscard]] size_t reproductive_cooldown_calculator() const { return reproductive_cooldown / m_cells_.size(); }
	[[nodiscard]] int get_cell_count() const { return static_cast<int>(m_cells_.size()); }
	[[nodiscard]] int get_spring_count() const { return static_cast<int>(m_springs_.size()); }

	[[nodiscard]] int calc_frames_alive()
	{
		return m_cells_[0].frames_alive_; // todo
	}

	// information setting
	void move_center_location_to(const sf::Vector2f new_center);

	void set_protozoa_attributes(const Protozoa* other)
	{
		m_cells_ = other->m_cells_;
		m_springs_ = other->m_springs_;
	}

	void add_cell();

	void remove_cell();


private:
	// mutating
	void mutate_existing_cells(float mut_rate = 0.f, float mut_range = 0.f);
	void mutate_existing_springs(float mut_rate = 0.f, float mut_range = 0.f);

public:
	bool run_separation(Protozoa* new_protozoa)
	{
		if (m_cells_.size() <= 1)
			return false;

		// BFS from cell 0 to find all cells reachable via springs
		std::vector<bool> visited(m_cells_.size(), false);
		std::queue<uint8_t> queue;
		queue.push(0);
		visited[0] = true;

		while (!queue.empty())
		{
			const uint8_t current = queue.front();
			queue.pop();

			for (const Spring& spring : m_springs_)
			{
				if (spring.cell_A_id == current && !visited[spring.cell_B_id])
				{
					visited[spring.cell_B_id] = true;
					queue.push(spring.cell_B_id);
				}
				else if (spring.cell_B_id == current && !visited[spring.cell_A_id])
				{
					visited[spring.cell_A_id] = true;
					queue.push(spring.cell_A_id);
				}
			}
		}

		// If every cell was reached, no split has occurred
		const bool all_connected = std::all_of(visited.begin(), visited.end(), [](bool v) { return v; });
		if (all_connected)
			return false;

		// Build old_id -> new_id remapping tables for each fragment
		std::vector<int> this_remap(m_cells_.size(), -1);
		std::vector<int> other_remap(m_cells_.size(), -1);

		std::vector<Cell>   this_cells, other_cells;
		std::vector<Spring> this_springs, other_springs;

		for (size_t i = 0; i < m_cells_.size(); ++i)
		{
			Cell c = m_cells_[i];
			if (visited[i])
			{
				this_remap[i] = static_cast<int>(this_cells.size());
				c.id = static_cast<uint8_t>(this_cells.size());
				this_cells.push_back(c);
			}
			else
			{
				other_remap[i] = static_cast<int>(other_cells.size());
				c.id = static_cast<uint8_t>(other_cells.size());
				other_cells.push_back(c);
			}
		}

		// Remap springs into the correct fragment; cross-fragment springs are discarded
		// (they no longer exist — that edge was the broken link that caused the split)
		for (const Spring& spring : m_springs_)
		{
			const bool a_stays = visited[spring.cell_A_id];
			const bool b_stays = visited[spring.cell_B_id];

			if (a_stays && b_stays)
			{
				Spring s = spring;
				s.cell_A_id = static_cast<uint8_t>(this_remap[spring.cell_A_id]);
				s.cell_B_id = static_cast<uint8_t>(this_remap[spring.cell_B_id]);
				s.id = static_cast<uint8_t>(this_springs.size());
				this_springs.push_back(s);
			}
			else if (!a_stays && !b_stays)
			{
				Spring s = spring;
				s.cell_A_id = static_cast<uint8_t>(other_remap[spring.cell_A_id]);
				s.cell_B_id = static_cast<uint8_t>(other_remap[spring.cell_B_id]);
				s.id = static_cast<uint8_t>(other_springs.size());
				other_springs.push_back(s);
			}
		}

		// Commit — this protozoa keeps the component containing cell 0
		m_cells_ = std::move(this_cells);
		m_springs_ = std::move(this_springs);

		// The new protozoa gets the disconnected fragment
		new_protozoa->m_cells_ = std::move(other_cells);
		new_protozoa->m_springs_ = std::move(other_springs);
		new_protozoa->active = true;

		return true;
	}


public:
	static sf::Rect<float> calc_protozoa_bounds(const Protozoa* protozoa)
	{
		const auto& cells = protozoa->get_cells();
		if (cells.empty())
			return {};

		const sf::Vector2f first_pos = cells[0].get_pos();
		float min_x = first_pos.x, max_x = min_x;
		float min_y = first_pos.y, max_y = min_y;

		for (const Cell& cell : cells)
		{
			const sf::Vector2f pos = cell.get_pos();
			min_x = std::min(min_x, pos.x - cell.radius);
			max_x = std::max(max_x, pos.x + cell.radius);
			min_y = std::min(min_y, pos.y - cell.radius);
			max_y = std::max(max_y, pos.y + cell.radius);
		}

		return { {min_x, min_y}, {max_x - min_x, max_y - min_y} };
	}
};
