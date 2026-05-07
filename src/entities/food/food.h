#pragma once

#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Color.hpp>


/* FoodManager

Food:
Reproduction:
When a food is born it can only reproduce when
1. its age surpasses the reproductive_threshold
2. it hasnt reproduced in a specified amount of frames (reproductive_cooldown)
3. The world food limit hasnt already been reached
- The chance of a food spawning is proportional to how much food there is in the world,
the more food there is the less chance there is for a new one to spawn.
- Every food item gets an opportunity to reproduce
- when a new food is made, it is spawned near its parent

Death:
- Food has a chance to die every frame after it reaches a certain age (death_age)
- it can also die by beng eaten by a protozoa

Nutrition:
- Every food starts with a certain amount of nutrients (initial_nutrients)
- Every frame the nutrients increase by a certain amount until they reach a maximum (final_nutrients)
- The nutrients of a food are what the protozoa gain when they eat it
*/

struct Food
{
	int id = 0;
	int age = 0;
	int time_since_last_reproduced = 0;

	float nutrients = 0.0f;

	sf::Vector2f position{};
	sf::Vector2f velocity{};
	sf::Color color{};

	bool active = true;
};