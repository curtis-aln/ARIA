#pragma once

#include <SFML/Graphics.hpp>

inline sf::Texture generateCircleTexture(float radius)
{
    const auto r = static_cast<unsigned>(radius * 2.f);
    sf::RenderTexture renderTexture({ r, r });
    sf::CircleShape circle(radius);
    circle.setFillColor(sf::Color::White);
    circle.setOrigin({ radius, radius });
    circle.setPosition({ radius, radius });
    renderTexture.clear(sf::Color::Transparent);
    renderTexture.draw(circle);
    renderTexture.display();

    sf::Texture tex = renderTexture.getTexture(); // copy BEFORE RenderTexture destructs
    return tex;
}


class CircleBatchRenderer
{
    sf::RenderWindow* window_ = nullptr;

    // the texture is reused in the vertex array 
    sf::Texture texture;
    sf::VertexArray vertex_array{ sf::PrimitiveType::Triangles };

    size_t max_circles_ = 0;
    size_t circle_count_ = 0;
    float tex_size = 0.f;

	// keeping pointers to the data vectors to avoid passing them every frame
    std::vector<sf::Color> colors_{};
	std::vector<float> positions_x_{};
	std::vector<float> positions_y_{};
	std::vector<float> radii_{};

public:
    explicit CircleBatchRenderer(sf::RenderWindow* window, float circle_texture_radius, size_t max_circles)
	: window_(window), max_circles_(max_circles)
    {
        texture = generateCircleTexture(circle_texture_radius);
        texture.setSmooth(true);
    }

    void set_colors(const std::vector<sf::Color>& colors) { colors_ = colors; }
    void set_positions_x(const std::vector<float>& pos_x) { positions_x_ = pos_x; }
    void set_positions_y(const std::vector<float>& pos_y) { positions_y_ = pos_y; }
    void set_radii(const std::vector<float>& radii) { radii_ = radii; }
    void set_size(size_t size) { circle_count_ = size; }

    void update()
    {
        circle_count_ = std::min({ circle_count_, max_circles_ });
        const size_t vertex_count = circle_count_ * 6;

        vertex_array.resize(vertex_count);

        tex_size = static_cast<float>(texture.getSize().x);
        const float u0 = 0.f, v0 = 0.f;
        const float u1 = tex_size, v1 = tex_size;

        for (size_t i = 0; i < circle_count_; ++i)
        {
            const size_t base = i * 6;
            const float pos_x = (positions_x_)[i];
            const float pos_y = (positions_y_)[i];
            const float r = (radii_)[i];
            const sf::Color col = (colors_)[i];

            vertex_array[base + 0] = {.position = { pos_x - r, pos_y - r }, .color = col, .texCoords = {u0, v0} };
            vertex_array[base + 1] = {.position = { pos_x + r, pos_y - r }, .color = col, .texCoords = {u1, v0} };
            vertex_array[base + 2] = {.position = { pos_x + r, pos_y + r }, .color = col, .texCoords = {u1, v1} };
            vertex_array[base + 3] = {.position = { pos_x - r, pos_y - r }, .color = col, .texCoords = {u0, v0} };
            vertex_array[base + 4] = {.position = { pos_x + r, pos_y + r }, .color = col, .texCoords = {u1, v1} };
            vertex_array[base + 5] = {.position = { pos_x - r, pos_y + r }, .color = col, .texCoords = {u0, v1} };

            
        }
    }

    void render()
    {
        window_->draw(vertex_array, &texture);
    }
};