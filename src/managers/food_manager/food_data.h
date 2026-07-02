#pragma once

struct FoodData
{
	std::vector<sf::Vector2f> positions;
	std::vector<sf::Color> colors;
	std::vector<float> radii;
	size_t active_count = 0;
};