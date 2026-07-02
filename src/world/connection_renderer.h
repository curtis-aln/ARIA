#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <cmath>
#include <algorithm>

/*
This class renders the connections between cells
It takes in a vector of Connections, and renders them as lines between the two positions
It adds circles to the ends of the lines to make smoother
The Thickness of the lines is preportional to the distance between the two positions
The color of the lines is preportional to the distance too

no_curves: when the zoom is low, the lines are rendered as straight lines between the two positions
*/

struct ConnectionSettings
{
	inline static float min_thickness = 20.f;
	inline static float max_thickness = 50.f;

	inline static sf::Color min_color = { 122, 168, 88, 170 };   // moss green
	inline static sf::Color max_color = { 180, 240, 50, 170 };    // burnt rust

	// triangles per rounded cap fan (only used when no_curves == false)
	inline static int cap_segments = 18;

	// worst-case connection count, used to pre-warm vertex array capacity
	inline static size_t max_connections_hint = 3000;
};

struct Connection
{
	sf::Vector2f pos_a;
	sf::Vector2f pos_b;

	float min_dist;
	float max_dist;

	float tension; // for colors
};

class ConnectionRenderer
{
	sf::VertexArray body_verts_{ sf::PrimitiveType::Triangles };
	sf::VertexArray cap_verts_{ sf::PrimitiveType::Triangles };

public:
	ConnectionRenderer()
	{
		// Pre-warm both vertex arrays to their worst-case size so the first few
		// frames (while the population is ramping up) don't trigger internal
		// reallocations. resize(0) afterwards keeps the reserved capacity.
		body_verts_.resize(ConnectionSettings::max_connections_hint * 6);
		body_verts_.resize(0);

		const size_t verts_per_cap = static_cast<size_t>(ConnectionSettings::cap_segments) * 3;
		cap_verts_.resize(ConnectionSettings::max_connections_hint * 2 * verts_per_cap);
		cap_verts_.resize(0);
	}

	// Rebuilds the internal vertex buffers from the given connections.
	// Call once per frame, before drawing, with the latest connection list.
	void update(const std::vector<Connection>& connections, bool no_curves)
	{
		const size_t count = connections.size();

		body_verts_.resize(count * 6);

		const size_t verts_per_cap = static_cast<size_t>(ConnectionSettings::cap_segments) * 3;
		cap_verts_.resize(no_curves ? 0 : count * 2 * verts_per_cap);

		size_t body_i = 0;
		size_t cap_i = 0;

		for (const Connection& conn : connections)
		{
			const RenderInfo info = compute_render_info(conn);

			build_body(conn, info, body_i);

			if (!no_curves)
			{
				build_cap(conn.pos_a, info, cap_i);
				build_cap(conn.pos_b, info, cap_i);
			}
		}
	}

	void draw(sf::RenderTarget& target) const
	{
		target.draw(body_verts_);
		if (cap_verts_.getVertexCount() > 0)
			target.draw(cap_verts_);
	}

private:
	struct RenderInfo
	{
		float thickness;
		sf::Color color;
		sf::Vector2f normal; // unit-length perpendicular to the connection, zero if degenerate
	};

	static float distance_ratio(const float dist, float min_dist, float max_dist)
	{
		const float range = max_dist - min_dist;
		if (range <= 0.f)
			return 0.f;

		const float t = (dist - min_dist) / range;
		return std::clamp(t, 0.f, 1.f);
	}

	static sf::Color lerp_color(const sf::Color& a, const sf::Color& b, const float t)
	{
		return sf::Color(
			static_cast<std::uint8_t>(a.r + (b.r - a.r) * t),
			static_cast<std::uint8_t>(a.g + (b.g - a.g) * t),
			static_cast<std::uint8_t>(a.b + (b.b - a.b) * t),
			static_cast<std::uint8_t>(a.a + (b.a - a.a) * t)
		);
	}

	static RenderInfo compute_render_info(const Connection& conn)
	{
		const sf::Vector2f diff = conn.pos_b - conn.pos_a;
		const float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);

		const float t = distance_ratio(dist, conn.min_dist, conn.max_dist);
		const float thickness = ConnectionSettings::min_thickness +
			(ConnectionSettings::max_thickness - ConnectionSettings::min_thickness) * t;

		const sf::Color color = lerp_color(ConnectionSettings::min_color, ConnectionSettings::max_color, conn.tension);

		sf::Vector2f normal{ 0.f, 0.f };
		if (dist > 0.0001f)
			normal = { -diff.y / dist, diff.x / dist };

		return { thickness, color, normal };
	}

	void build_body(const Connection& conn, const RenderInfo& info, size_t& vertex_index)
	{
		const sf::Vector2f offset = info.normal * (info.thickness * 0.5f);

		const sf::Vector2f p1 = conn.pos_a + offset;
		const sf::Vector2f p2 = conn.pos_a - offset;
		const sf::Vector2f p3 = conn.pos_b + offset;
		const sf::Vector2f p4 = conn.pos_b - offset;

		body_verts_[vertex_index + 0] = sf::Vertex{ p1, info.color };
		body_verts_[vertex_index + 1] = sf::Vertex{ p2, info.color };
		body_verts_[vertex_index + 2] = sf::Vertex{ p3, info.color };

		body_verts_[vertex_index + 3] = sf::Vertex{ p2, info.color };
		body_verts_[vertex_index + 4] = sf::Vertex{ p4, info.color };
		body_verts_[vertex_index + 5] = sf::Vertex{ p3, info.color };

		vertex_index += 6;
	}

	void build_cap(const sf::Vector2f& center, const RenderInfo& info, size_t& vertex_index)
	{
		const float radius = info.thickness * 0.5f;
		const int segments = ConnectionSettings::cap_segments;
		const float step = 6.2831853f / static_cast<float>(segments);

		for (int i = 0; i < segments; ++i)
		{
			const float a0 = step * static_cast<float>(i);
			const float a1 = step * static_cast<float>(i + 1);

			const sf::Vector2f p0 = center + sf::Vector2f{ std::cos(a0) * radius, std::sin(a0) * radius };
			const sf::Vector2f p1 = center + sf::Vector2f{ std::cos(a1) * radius, std::sin(a1) * radius };

			cap_verts_[vertex_index + 0] = sf::Vertex{ center, info.color };
			cap_verts_[vertex_index + 1] = sf::Vertex{ p0, info.color };
			cap_verts_[vertex_index + 2] = sf::Vertex{ p1, info.color };

			vertex_index += 3;
		}
	}
};