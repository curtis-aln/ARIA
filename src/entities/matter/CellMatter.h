#pragma once
#include "../cell/cell.h"
#include "matter_settings.h"

struct CellMatter : CellMatterSettings
{
	bool dead = false;

	uint32_t id_ = 0;
	uint32_t body_id_ = 0;

	uint8_t  inner_gray_ = 0;
	uint8_t  outer_gray_ = 0;
	uint16_t time_alive = 0;

	// the color the cell had at the moment it died, faded out over color_transition_frames
	sf::Color original_inner_{};
	sf::Color original_outer_{};

	// cached results, only recomputed when time_alive advances during the fade window
	sf::Color cached_inner_{};
	sf::Color cached_outer_{};
	bool fade_complete_ = false; // once true, cached_* are final and update() stops touching them

	sf::Color inner_color() const { return cached_inner_; }
	sf::Color outer_color() const { return cached_outer_; }

	void update(Body* body)
	{
		time_alive++;
		body->velocity_ *= friction;

		dead = time_alive > max_time_to_live;

		if (!fade_complete_)
			recompute_colors();
	}

	void reset_cell_matter()
	{
		time_alive = 0;
		dead = false;
	}

	void cell_to_matter(Body* body, sf::Color original_color_inner, sf::Color original_color_outer)
	{
		inner_gray_ = Random::rand_range<uint8_t>(min_gray, max_gray);
		outer_gray_ = Random::rand_range<uint8_t>(min_gray, max_gray);

		original_inner_ = original_color_inner;
		original_outer_ = original_color_outer;

		time_alive = 0;
		fade_complete_ = false;

		body->radius_ = Random::rand_range<uint8_t>(min_radius, max_radius);

		body_id_ = body->id_;

		recompute_colors(); // seed cache immediately so inner_color()/outer_color() are valid right away
	}

private:
	void recompute_colors()
	{
		if (time_alive >= static_cast<uint16_t>(color_transition_frames))
		{
			// fade finished: lock in the flat gray targets, never touch this again
			cached_inner_ = sf::Color(inner_gray_, inner_gray_, inner_gray_, static_cast<uint8_t>(transparency_inner));
			cached_outer_ = sf::Color(outer_gray_, outer_gray_, outer_gray_, static_cast<uint8_t>(transparency_outer));
			fade_complete_ = true;
			return;
		}

		const float t = static_cast<float>(time_alive) * inv_color_transition_frames; // in [0,1), no clamp needed

		cached_inner_ = lerp_to_gray(original_inner_, inner_gray_, transparency_inner, t);
		cached_outer_ = lerp_to_gray(original_outer_, outer_gray_, transparency_outer, t);
	}

	static sf::Color lerp_to_gray(sf::Color original, uint8_t target_gray, float alpha, float t)
	{
		const float r = original.r + (target_gray - static_cast<float>(original.r)) * t;
		const float g = original.g + (target_gray - static_cast<float>(original.g)) * t;
		const float b = original.b + (target_gray - static_cast<float>(original.b)) * t;

		return sf::Color(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), static_cast<uint8_t>(alpha));
	}
};