#pragma once

#include <cmath> 
#include <chrono>

inline std::string format_variables(const std::vector<std::pair<std::string, double>>& variables) {
	std::ostringstream oss;
	oss.precision(2);
	oss << std::fixed;
	for (const auto& [fst, snd] : variables) {
		oss << fst << ": " << snd << ", ";
	}
	const std::string result = oss.str();
	// Remove the last comma and space
	return result.substr(0, result.size() - 2);
}


inline void caption_frame_rate(sf::RenderWindow& window, const std::string& title, const int rendering_frame_rate)
{
	std::ostringstream oss;
	oss << title << " " << rendering_frame_rate << "fps \n";
	const std::string string_frame_rate = oss.str();
	window.setTitle(string_frame_rate);
}


template<typename T>
std::string denary_to_str(T number, const size_t precision, const bool round=true)
{

	if (round)
	{
		const float multiplier = std::pow(10.f, precision);
		number = std::round(number * multiplier) / multiplier;
	}

	// Convert the double to a string with the specified precision
	std::string result = std::to_string(number);

	if (const size_t dot_position = result.find('.'); dot_position != std::string::npos)
	{
		// Check if there are enough characters after the dot
		if (result.length() - dot_position > precision + 1)
		{
			result = result.substr(0, dot_position + precision + 1);
		}
	}

	return result;
}

inline constexpr float pi = 3.14159265358979323846264338327950f;


inline void normalize_angle(float& angle_radians)
{
	while (angle_radians < 0.f)
	{
		angle_radians += 2.f * pi;
	}
	while (angle_radians >= 2.f * pi)
	{
		angle_radians -= 2.f * pi;
	}
}