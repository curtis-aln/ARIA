#pragma once

struct FoodData
{
	std::vector<float> positions_x;
	std::vector<float> positions_y;
	std::vector<sf::Color> colors;
	std::vector<float> radii;
	size_t active_count = 0;
};