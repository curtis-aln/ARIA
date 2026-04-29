#include "world.h"

#include "../settings.h"
#include "../Utils/utility_SFML.h"


void World::draw_protozoa_debug(const SimSnapshot& snapshot, Font* font)
{
    // Fetching information
	const Protozoa* protozoa = &snapshot.protozoa;
    const sf::FloatRect bounds = calc_protozoa_bounds(protozoa);

    // Springs are drawn first so that they are under the cells
    draw_springs(protozoa, true);

    if (snapshot.toggles.skeleton_mode)
        draw_cell_outlines(protozoa);


    if (snapshot.toggles.show_bounding_boxes)
        draw_protozoa_bounding_box(bounds, *m_window_);

    draw_cell_physical_information(protozoa, font);
    //draw_spring_information(protozoa, font);

    if (snapshot.toggles.show_connections)
    {
        nearby_food_information(protozoa);
    }
}


void World::draw_cell_outlines(const Protozoa* protozoa)
{
    sf::CircleShape circle_outline;
    circle_outline.setPointCount(30); // Reduce aliasing, set once
    for (const Cell& cell : protozoa->get_cells())
    {
        const sf::Vector2f pos = cell.get_pos();
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

void World::nearby_food_information(const Protozoa* protozoa) const
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


void World::draw_cell_physical_information(const Protozoa* protozoa, Font* font) const
{
    // for each cell we draw its bounding box
    for (const Cell& cell : protozoa->get_cells())
    {
        const sf::Vector2f& pos = cell.get_pos();
		const sf::Vector2f& vel = cell.get_vel();
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
        font->draw(top_left, "id: " + std::to_string(cell.id), false);
    }
}


void World::draw_springs(const Protozoa* protozoa, const bool thick_lines) const
{
    for (const Spring& spring : protozoa->get_springs())
    {
        const sf::Vector2f& pos1 = protozoa->get_cells()[spring.cell_A_id].get_pos();
        const sf::Vector2f& pos2 = protozoa->get_cells()[spring.cell_B_id].get_pos();

        if (thick_lines)
        {
            // the outline color should be that of cell a, the inline cololour should be that of cell b
            const sf::Color outline_color = protozoa->get_cells()[spring.cell_A_id].get_outer_color();
            const sf::Color fill_color = protozoa->get_cells()[spring.cell_B_id].get_outer_color();

            draw_thick_line(*m_window_, pos1, pos2, GraphicalSettings::spring_thickness,
                GraphicalSettings::spring_outline_thickness, fill_color, outline_color);
        }
        else
        {
            m_window_->draw(make_line(pos1, pos2, sf::Color::Magenta));
        }
    }
}

void World::draw_spring_information(const Protozoa* protozoa, Font* font) const
{

}


int World::check_mouse_press(const Protozoa* protozoa, const sf::Vector2f mousePosition, const bool tolerance_check) const
{
    for (const Cell& cell : protozoa->get_cells())
    {
        const float dist_sq = (cell.get_pos() - mousePosition).lengthSquared();
        float tollarance_factor = 1.2f;
        const float rad = cell.radius * tollarance_factor;
        if (dist_sq < rad * rad)
        {
            return cell.id;
        }
    }

    return -1;
}


const Cell* World::get_selected_cell(const Protozoa* protozoa, const sf::Vector2f mouse_pos)
{
    if (!check_mouse_press(protozoa, mouse_pos, true))
        return nullptr;

    for (const Cell& cell : protozoa->get_cells())
    {
        const float dist_sq = (cell.get_pos() - mouse_pos).lengthSquared();
        const float rad = cell.radius;
        if (dist_sq < rad * rad)
        {
            return &cell;
        }
    }

    return nullptr;
}

