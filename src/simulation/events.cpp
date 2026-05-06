#include "simulation.h"

// ---- Mouse helpers ------------------------------------------------

bool Simulation::try_select_protozoa(const sf::Vector2f& cam_pos)
{
	return m_world_.handle_mouse_click(cam_pos);
}

void Simulation::handle_mouse_press(const sf::Vector2f& cam_pos)
{
	if (try_select_protozoa(cam_pos))
		mouse_pressed_event = true;
	else
	{
		m_world_.deselect_cell();
		camera_.begin_pan();  // start pan only if we didn't click an organism
	}
}

void Simulation::handle_mouse_release()
{
	//m_world_.deselect_protozoa();
	mouse_pressed_event = false;
	camera_.end_pan();
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
	else if (event.is<sf::Event::MouseButtonPressed>())
	{
		if (!ImGui::GetIO().WantCaptureMouse)  // don't interact with sim if imgui is focused
			handle_mouse_press(cam_pos);
	}
	else if (event.is<sf::Event::MouseButtonReleased>())
		handle_mouse_release();
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