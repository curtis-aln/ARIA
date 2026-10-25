#include "context/sim_command.h"
#include "settings/settings.h"
#include "simulation.h"
#include <imgui.h>
#include <imgui-SFML.h>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>

// ---- Mouse helpers ------------------------------------------------


bool Simulation::try_select_protozoa(const sf::Vector2f& cam_pos)
{
	return m_world_.handle_mouse_click(cam_pos);
}

void Simulation::handle_left_click(const sf::Vector2f& cam_pos)
{
	/* Select / Deselect protozoa / Camera pan */
	if (camera_.get_current_zoom() > min_zoom_to_select_protozoa && try_select_protozoa(cam_pos))
		m_control_panel_.select_tab("Organism");
	else if (m_world_.get_cell_manager()->deselect_cell())
		m_control_panel_.select_tab("Simulation");

	camera_.begin_pan();
}

void Simulation::handle_left_release()
{
	camera_.end_pan();
}

void Simulation::handle_right_release()
{
	m_world_.dragging = false; // release the protozoa if we were dragging one
	m_world_.get_cell_manager()->deselect_cell();
}

// ---- Keyboard helpers ---------------------------------------------

void Simulation::handle_pause_toggle()
{
	/* toggle flip paused */
	m_world_.world_toggles.paused = !m_world_.world_toggles.paused;

	if (!m_world_.world_toggles.paused)
		m_world_.world_toggles.m_tick_frame_time = false;
}

void Simulation::handle_keyboard_events(const sf::Keyboard::Key& event_key_code)
{
	switch (event_key_code)
	{
	case sf::Keyboard::Key::Escape: running = false;              break;
	case sf::Keyboard::Key::Space:  handle_pause_toggle();        break;
	case sf::Keyboard::Key::R:      toggles_.m_rendering_ = !toggles_.m_rendering_; break;
	case sf::Keyboard::Key::Q:      toggles_.hide_panels = !toggles_.hide_panels; break;
	case sf::Keyboard::Key::O:
		m_world_.world_toggles.m_tick_frame_time = true;
		m_world_.world_toggles.paused = true;
		break;
	case sf::Keyboard::Key::F11: toggle_fullscreen(); break;
	case sf::Keyboard::Key::F9:  toggle_titlebar();   break;
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

	camera_.update();
}

void Simulation::handle_simulation_event(SimCommand& cmd)
{
	switch (cmd.type)
	{
	case CommandType::SetUpdatingFrameRate:
		sim_state_.max_frame_rate_updating = cmd.float_val;
		updating_clock_.set_target_fps(cmd.float_val);
		break;

	case CommandType::SetRenderingFrameRate:
		sim_state_.max_frame_rate_rendering = cmd.float_val;
		rendering_clock_.set_target_fps(cmd.float_val);
		break;

	case CommandType::SetMouseMode:
		m_world_.get_statistics().mouse_mode = cmd.int_val;
		break;

	case CommandType::SetZoomLevel:
		camera_.set_zoom(cmd.float_val,
			camera_.window_pos_to_world_pos(sf::Vector2f{ (float)(videoMode.size.x / 2u), (float)(videoMode.size.y / 2u) }));
		break;

	case CommandType::SetWorldToggles:
		m_world_.world_toggles = cmd.world_toggles;
		break;

	case CommandType::SetToSimulationTab:
		m_control_panel_.select_tab("Simulation");
		break;

	case CommandType::SetToOrganismTab:
		m_control_panel_.select_tab("Organisms");
		break;

	case CommandType::SetBackgroundPreset:
		SimulationSettings::bg_preset_index = cmd.int_val;
		SimulationSettings::window_color = SimulationSettings::bg_presets[cmd.int_val].colors[0];
		break;

	case CommandType::SetCustomBackground:
		SimulationSettings::bg_preset_index = static_cast<int>(SimulationSettings::bg_presets.size()) - 1; // "Custom" slot
		SimulationSettings::window_color = cmd.color_val_a;
		break;
	}
}