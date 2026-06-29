#include "simulation.h"

// ---- Mouse helpers ------------------------------------------------

bool Simulation::try_select_protozoa(const sf::Vector2f& cam_pos)
{
	return m_world_.handle_mouse_click(cam_pos);
}

void Simulation::handle_left_click(const sf::Vector2f& cam_pos)
{
	// when a mouse button is pressed down it is either to pan the screen or to interact with an organism
	if (try_select_protozoa(cam_pos))
	{
		left_mouse_pressed_event = true;
	}
	else
	{
		m_world_.get_cell_manager()->deselect_cell();
		camera_.begin_pan();  // start pan only if we didn't click an organism
	}
}

void Simulation::handle_right_click(const sf::Vector2f& cam_pos)
{
	// right click allows you to drag protozoa around the screen
	right_mouse_pressed_event = true;

	// we should only drag a protozoa if our mouse is within the radius of one
	//if (try_select_protozoa(cam_pos))
	//{
	//	m_world_.should_drag_protozoa_ = true;
	//}
}



void Simulation::handle_left_release()
{
	//m_world_.deselect_protozoa();
	left_mouse_pressed_event = false;
	camera_.end_pan();
}

void Simulation::handle_right_release()
{
	right_mouse_pressed_event = false;
	m_world_.should_drag_protozoa_ = false; // release the protozoa if we were dragging one
}

// ---- Keyboard helpers ---------------------------------------------

void Simulation::handle_pause_toggle()
{
	m_world_.toggles.paused = !m_world_.toggles.paused;

	if (!m_world_.toggles.paused)
	{
		m_world_.toggles.m_tick_frame_time = false;
	}
}

void Simulation::handle_keyboard_events(const sf::Keyboard::Key& event_key_code)
{
	switch (event_key_code)
	{
	case sf::Keyboard::Key::Escape: running = false;              break;
	case sf::Keyboard::Key::Space:  handle_pause_toggle();        break;
	case sf::Keyboard::Key::R:      m_world_.toggles.m_rendering_ = !m_world_.toggles.m_rendering_; break;
	case sf::Keyboard::Key::Q:      m_world_.toggles.hide_panels = !m_world_.toggles.hide_panels; break;
	case sf::Keyboard::Key::O:      
		m_world_.toggles.m_tick_frame_time = true;
		m_world_.toggles.paused = true;
		break;
	default: break;
	}
}

// ---- Event dispatch -----------------------------------------------

void Simulation::dispatch_event(const sf::Event& event, const sf::Vector2f& cam_pos)
{
	if (event.is<sf::Event::Closed>())
		running = false;

	else if (const auto* key = event.getIf<sf::Event::KeyPressed>())
	{
		handle_keyboard_events(key->code);
		m_world_.keyboardEvents(key->code);
	}
	else if (const auto* scroll = event.getIf<sf::Event::MouseWheelScrolled>())
	{
		if (!ImGui::GetIO().WantCaptureMouse)  // don't zoom sim if imgui is using scroll
			camera_.zoom(scroll->delta);
	}
	else if (const auto* mouse = event.getIf<sf::Event::MouseButtonPressed>())
	{
		if (!ImGui::GetIO().WantCaptureMouse)
		{
			if (mouse->button == sf::Mouse::Button::Left)
				handle_left_click(cam_pos);
			else if (mouse->button == sf::Mouse::Button::Right)
				handle_right_click(cam_pos);
		}
	}
	else if (const auto* mouse = event.getIf<sf::Event::MouseButtonReleased>())
	{
		if (mouse->button == sf::Mouse::Button::Left)
			handle_left_release();
		else if (mouse->button == sf::Mouse::Button::Right)
			handle_right_release();
	}

}

// ---- Top-level ----------------------------------------------------

void Simulation::handle_events()
{
	const sf::Vector2f cam_pos = camera_.get_world_mouse_pos();

	while (const std::optional event = m_window_.pollEvent())
	{
		ImGui::SFML::ProcessEvent(m_window_, *event);
		dispatch_event(*event, cam_pos);
	}

	camera_.update(m_clock_.get_delta_time());
}