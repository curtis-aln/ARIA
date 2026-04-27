#include "ProtozoaManager.h"

#include "../settings.h"
#include "../Utils/utility_SFML.h"

void ProtozoaManager::render_protozoa_springs(Protozoa* protozoa)
{
    for (Cell& cell : protozoa->get_cells())
    {
        render_cell_connections(protozoa, cell, true);
    }
}

void ProtozoaManager::render_debug(Protozoa* protozoa, Font* font, const bool skeleton_mode, const bool show_connections, const bool show_bounding_boxes)
{
    // skeleton mode just hides the cell bodies and leaves only the outlines of the cells
    if (skeleton_mode)
    {
        render_protozoa_springs(protozoa);
        draw_cell_outlines(protozoa);
    }


    // This is the bounding box of the protozoa, used for external collision events
    if (show_bounding_boxes)
    {
        //draw_protozoa_bounding_box(m_personal_bounds_, *m_window_);
    }

    draw_cell_physics(protozoa, font);
    draw_spring_information(protozoa, font);

    if (show_connections)
    {
        nearby_food_information(protozoa);
    }
}

void ProtozoaManager::draw_cell_outlines(Protozoa* protozoa)
{
    sf::CircleShape circle_outline;
    circle_outline.setPointCount(30); // Reduce aliasing, set once
    for (Cell& cell : protozoa->get_cells())
    {
        const sf::Vector2f pos = cell.position_;
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

void ProtozoaManager::nearby_food_information(Protozoa* protozoa) const
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


void ProtozoaManager::render_cell_connections(Protozoa* protozoa, Cell& cell, const bool thick_lines) const
{
    for (const Spring& spring : protozoa->get_springs())
    {
        const sf::Vector2f& pos1 = protozoa->get_cells()[spring.cell_A_id].position_;
        const sf::Vector2f& pos2 = protozoa->get_cells()[spring.cell_B_id].position_;

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


void ProtozoaManager::draw_cell_physics(Protozoa* protozoa, Font* font)
{
    // for each cell we draw its bounding box
    for (Cell& cell : protozoa->get_cells())
    {
        const sf::Vector2f& pos = cell.position_;
        const float rad = cell.radius;

        // rendering the bounding boxes
        const sf::FloatRect rect = { {pos.x - rad, pos.y - rad}, {rad * 2, rad * 2} };
        draw_protozoa_bounding_box(rect, *m_window_);
        render_cell_connections(protozoa, cell, false);

        // drawing the direction of the cell
        const float arrow_length = std::min(rad * 4, cell.velocity_.length() * rad);
        draw_direction(*m_window_, pos, cell.velocity_, arrow_length, 6, 10,
            { 200, 220, 200 }, { 190, 200, 190 });

        // drawing cell stats
        const auto top_left = rect.position;
        const auto spacing = font->get_text_size("0").y;
        const sf::Vector2f offset = { 0, spacing };
        font->draw(top_left, "id: " + std::to_string(cell.id), false);
    }
}




void ProtozoaManager::draw_spring_information(Protozoa* protozoa, Font* font) const
{

}


int ProtozoaManager::check_mouse_press(Protozoa* protozoa, const sf::Vector2f mousePosition, const bool tolerance_check) const
{
    for (const Cell& cell : protozoa->get_cells())
    {
        const float dist_sq = (cell.position_ - mousePosition).lengthSquared();
        float tollarance_factor = 1.2f;
        const float rad = cell.radius * tollarance_factor;
        if (dist_sq < rad * rad)
        {
            return cell.id;
        }
    }

    return -1;
}


Cell* ProtozoaManager::get_selected_cell(Protozoa* protozoa, const sf::Vector2f mouse_pos)
{
    if (!check_mouse_press(protozoa, mouse_pos, true))
        return nullptr;

    for (Cell& cell : protozoa->get_cells())
    {
        const float dist_sq = (cell.position_ - mouse_pos).lengthSquared();
        const float rad = cell.radius;
        if (dist_sq < rad * rad)
        {
            return &cell;
        }
    }

    return nullptr;
}

