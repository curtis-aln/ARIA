#include "../world.h"

#include "../../simulation/settings/settings.h"
#include "../../Utils/utility_SFML.h"


void World::draw_protozoa_debug(const SimSnapshot& snapshot, Font* font)
{
	const OrganismTracker& protozoa = snapshot.protozoa_tracker;

	if (protozoa.is_active == false)
		return;

    if (snapshot.toggles.skeleton_mode)
        draw_cell_outlines(protozoa);


    if (snapshot.toggles.show_bounding_boxes)
        draw_protozoa_bounding_box(protozoa.bounds, *m_window_);

    draw_cell_physical_information(protozoa, font);
    //draw_spring_information(protozoa, font);

    if (snapshot.toggles.show_connections)
    {
        nearby_food_information(protozoa);
    }
}


void World::draw_cell_outlines(const OrganismTracker& protozoa)
{
    sf::CircleShape circle_outline;
    circle_outline.setPointCount(30); // Reduce aliasing, set once
    for (const Cell& cell : protozoa.cells)
    {
		Body* body = bodies_.at(cell.body_id_);

        const sf::Vector2f pos = body->position_;
        const float rad = cell.radius + GraphicalSettings::cell_outline_thickness;

        circle_outline.setRadius(rad);
        circle_outline.setFillColor({ 0, 0, 0 });
        circle_outline.setPosition(pos - sf::Vector2f{ rad, rad });
        m_window_->draw(circle_outline);

        circle_outline.setFillColor({ 255, 0, 255 });
        circle_outline.setRadius(rad / 3);
        circle_outline.setPosition(pos - sf::Vector2f{ rad / 3, rad / 3 });
        m_window_->draw(circle_outline);
    }
}

void World::nearby_food_information(const OrganismTracker& protozoa) const
{
    //const sf::Vector2f center = get_center();
    //static const sf::Color food_line_color{ 50, 153, 204, 100 };
    //for (const sf::Vector2f& pos : food_positions_nearby)
    //{
    //    m_window_->draw(make_line(center, pos, food_line_color));
    //}

    //for (const sf::Vector2f& pos : cell_positions_nearby)
    //{
    //    m_window_->draw(make_line(center, pos, sf::Color::Yellow));
    //}
}


void World::draw_cell_physical_information(const OrganismTracker& protozoa, Font* font) const
{
    // for each cell we draw its bounding box
    for (const Cell& cell : protozoa.cells)
    {
		const Body* body = bodies_.at(cell.body_id_);

        const sf::Vector2f& pos = body->position_;
		const sf::Vector2f& vel = body->velocity_;
		const float speed = vel.length();
        const float rad = cell.radius;

        // rendering the bounding boxes
        const sf::FloatRect rect = { {pos.x - rad, pos.y - rad}, {rad * 2, rad * 2} };
        draw_protozoa_bounding_box(rect, *m_window_);

        // drawing the direction of the cell
        const float arrow_length = std::min(rad * 4, speed * rad);
        draw_direction(*m_window_, pos, vel, arrow_length, 6, 10,
            { 200, 220, 200 }, { 190, 200, 190 });

        // drawing cell stats
        const auto top_left = rect.position;
        const auto spacing = font->get_text_size("0").y;
        const sf::Vector2f offset = { 0, spacing };
        font->draw(top_left, "id: " + std::to_string(cell.body_id_), false);
    }
}

void World::draw_spring_information(Font* font) const
{

}


int World::check_mouse_press(const OrganismTracker& protozoa, const sf::Vector2f mousePosition, const bool tolerance_check) const
{
    for (const Cell& cell : protozoa.cells)
    {
		const Body* body = bodies_.at(cell.body_id_);
        const float dist_sq = (body->position_ - mousePosition).lengthSquared();
        float tollarance_factor = 1.2f;
        const float rad = cell.radius * tollarance_factor;
        if (dist_sq < rad * rad)
        {
            return cell.body_id_;
        }
    }

    return -1;
}

