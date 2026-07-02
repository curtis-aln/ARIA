#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <iostream>

// Creates a grid of nested level N to give a frame of reference when navigating 2d space

// todo use recursion to sort the max nest level
class SFML_Grid
{
	sf::RenderWindow& window_;
	sf::FloatRect world_dimensions_;
	size_t max_nest_level_;

	sf::VertexBuffer vertexBuffer_{};
	bool shaderAvailable_ = false;

	// Shared across all grid instances so the shader is only compiled once.
	static sf::Shader& alphaShader(bool& available)
	{
		static sf::Shader shader;
		static const bool loaded = []
			{
				if (!sf::Shader::isAvailable())
					return false;

				static const std::string fragShader = R"(
				uniform float alpha;
				void main()
				{
					vec4 color = gl_Color;
					color.a *= alpha;
					gl_FragColor = color;
				}
			)";
				return shader.loadFromMemory(fragShader, sf::Shader::Type::Fragment);
			}();

		available = loaded;
		return shader;
	}

	static void addVerticalLine(std::vector<sf::Vertex>& vertices, float x, float yTop, float yBottom,
		float thickness, sf::Color color)
	{
		const float half = thickness * 0.5f;
		const float xLeft = x - half;
		const float xRight = x + half;

		vertices.push_back({ { xLeft,  yTop },    color });
		vertices.push_back({ { xRight, yTop },    color });
		vertices.push_back({ { xRight, yBottom }, color });

		vertices.push_back({ { xLeft,  yTop },    color });
		vertices.push_back({ { xRight, yBottom }, color });
		vertices.push_back({ { xLeft,  yBottom }, color });
	}

	static void addHorizontalLine(std::vector<sf::Vertex>& vertices, float y, float xLeft, float xRight,
		float thickness, sf::Color color)
	{
		const float half = thickness * 0.5f;
		const float yTop = y - half;
		const float yBottom = y + half;

		vertices.push_back({ { xLeft,  yTop },    color });
		vertices.push_back({ { xRight, yTop },    color });
		vertices.push_back({ { xRight, yBottom }, color });

		vertices.push_back({ { xLeft,  yTop },    color });
		vertices.push_back({ { xRight, yBottom }, color });
		vertices.push_back({ { xLeft,  yBottom }, color });
	}

public:
	SFML_Grid(sf::RenderWindow& window, const sf::FloatRect& world_dimensions, const size_t cells_x,
		const size_t max_nest_level = 3, const sf::Color color = { 75, 75, 75 },
		const float thickness = 1.f)
		: window_(window), world_dimensions_(world_dimensions), max_nest_level_(max_nest_level)
	{
		const float aspect_ratio = world_dimensions.size.x / world_dimensions.size.y;
		const size_t cells_y = static_cast<size_t>(static_cast<float>(cells_x) * aspect_ratio);
		const float cell_size = world_dimensions.size.x / static_cast<float>(cells_x);

		// +1 on each axis so the right-most and bottom-most edges actually get drawn.
		const size_t line_count_x = cells_x + 1;
		const size_t line_count_y = cells_y + 1;

		const float left = world_dimensions.position.x;
		const float top = world_dimensions.position.y;
		const float right = left + world_dimensions.size.x;
		const float bottom = top + world_dimensions.size.y;

		std::vector<sf::Vertex> vertices;
		vertices.reserve((line_count_x + line_count_y) * 6); // 6 verts per line (2 triangles)

		for (size_t x = 0; x < line_count_x; ++x)
		{
			const float posX = left + static_cast<float>(x) * cell_size;
			addVerticalLine(vertices, posX, top, bottom, thickness, color);
		}

		for (size_t y = 0; y < line_count_y; ++y)
		{
			const float posY = top + static_cast<float>(y) * cell_size;
			addHorizontalLine(vertices, posY, left, right, thickness, color);
		}

		vertexBuffer_ = sf::VertexBuffer(sf::PrimitiveType::Triangles, sf::VertexBuffer::Usage::Static);

		if (!vertexBuffer_.create(vertices.size()))
		{
			std::cout << "[Warning]: Failed to create vertex buffer for grid rendering." << std::endl;
		}

		if (!vertexBuffer_.update(vertices.data(), static_cast<unsigned int>(vertices.size()), 0))
		{
			std::cout << "[Warning]: Failed to update vertex buffer for grid rendering." << std::endl;
		}

		alphaShader(shaderAvailable_);
	}

	// alpha in [0, 1]; falls back to full-opacity draw if shaders aren't supported.
	void draw(float alpha = 1.f)
	{
		if (shaderAvailable_)
		{
			sf::Shader& shader = alphaShader(shaderAvailable_);
			shader.setUniform("alpha", alpha);

			sf::RenderStates states;
			states.shader = &shader;
			window_.draw(vertexBuffer_, states);
		}
		else
		{
			window_.draw(vertexBuffer_);
		}
	}
};