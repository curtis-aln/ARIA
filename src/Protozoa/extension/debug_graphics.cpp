#include "../Protozoa.h"
#include "../../settings.h"
#include "../../Utils/utility_SFML.h"

void Protozoa::render_protozoa_springs()
{
	for (Cell& cell : m_cells_)
	{
		render_cell_connections(cell, true);
	}
}

void Protozoa::render_debug(Font* font, const bool skeleton_mode, const bool show_connections, const bool show_bounding_boxes)
{
	// skeleton mode just hides the cell bodies and leaves only the outlines of the cells
	if (skeleton_mode)
	{
        render_protozoa_springs();
		draw_cell_outlines();
	}


	// This is the bounding box of the protozoa, used for external collision events
	if (show_bounding_boxes)
	{
		draw_protozoa_bounding_box(m_personal_bounds_, *m_window_);
	}

	draw_cell_physics(font);
	draw_spring_information(font);

	if (show_connections)
	{
		nearby_food_information();
	}
}

void Protozoa::draw_cell_outlines()
{
    sf::CircleShape circle_outline;
    circle_outline.setPointCount(30); // Reduce aliasing, set once
    for (Cell& cell : m_cells_)
    {
        const sf::Vector2f pos = cell.position_;
        const float rad = cell.radius + GraphicalSettings::cell_outline_thickness;

        circle_outline.setRadius(rad);
        circle_outline.setFillColor({ 0, 0, 0 });
        circle_outline.setPosition(pos - sf::Vector2f{rad, rad});
        m_window_->draw(circle_outline);

        circle_outline.setFillColor({ 255, 0, 255 });
        circle_outline.setRadius(rad / 3);
        circle_outline.setPosition(pos - sf::Vector2f{ rad / 3, rad / 3 });
        m_window_->draw(circle_outline);
    }
}

void Protozoa::nearby_food_information() const
{
	const sf::Vector2f center = get_center();
    static const sf::Color food_line_color{ 50, 153, 204, 100 };
    //for (const sf::Vector2f& pos : food_positions_nearby)
    //{
    //    m_window_->draw(make_line(center, pos, food_line_color));
    //}

    //for (const sf::Vector2f& pos : cell_positions_nearby)
    //{
    //    m_window_->draw(make_line(center, pos, sf::Color::Yellow));
    //}
}


void Protozoa::render_cell_connections(Cell& cell, const bool thick_lines) const
{
    for (const Spring& spring : m_springs_)
    {
        const sf::Vector2f& pos1 = m_cells_[spring.cell_A_id].position_;
        const sf::Vector2f& pos2 = m_cells_[spring.cell_B_id].position_;

        if (thick_lines)
        {
            // the outline color should be that of cell a, the inline cololour should be that of cell b
			const sf::Color outline_color = m_cells_[spring.cell_A_id].cell_outer_color;
			const sf::Color fill_color = m_cells_[spring.cell_B_id].cell_outer_color;

            draw_thick_line(*m_window_, pos1, pos2, GraphicalSettings::spring_thickness,
				GraphicalSettings::spring_outline_thickness, fill_color, outline_color);
        }
        else
        {
            m_window_->draw(make_line(pos1, pos2, sf::Color::Magenta));
        }
    }
}


void Protozoa::draw_cell_physics(Font* font)
{
	// for each cell we draw its bounding box
    for (Cell& cell : m_cells_)
    {
        const sf::Vector2f& pos = cell.position_;
        const float rad = cell.radius;

        // rendering the bounding boxes
		const sf::FloatRect rect = { {pos.x - rad, pos.y - rad}, {rad * 2, rad * 2} };
        draw_protozoa_bounding_box(rect, *m_window_);
        render_cell_connections(const_cast<Cell&>(cell));

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




void Protozoa::draw_spring_information(Font* font) const
{

}


int Protozoa::check_mouse_press(const sf::Vector2f mousePosition, const bool tolerance_check) const
{
	// This function checks if the mouse has selected any cell in the protozoa, if so it returns the id of the cell, otherwise it returns -1
	if (!m_personal_bounds_.contains(mousePosition)) // If the mouse is outside the protozoa bounds, we can skip checking each cell
        return -1;

    for (const Cell& cell : m_cells_)
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


Cell* Protozoa::get_selected_cell(const sf::Vector2f mouse_pos)
{
	if (!check_mouse_press(mouse_pos))
		return nullptr;
	
	for (Cell& cell : m_cells_)
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

