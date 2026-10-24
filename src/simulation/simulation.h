#pragma once
#include "../world/world.h"
#include "imgui/control_panel.h"
#include "imgui/tabs/graphs/population_history.h"
#include "settings/settings.h"

#include "../Utils/custom_clock.h"
#include "../Utils/UI/Camera.hpp"

#include <implot.h>

#include "context/sim_command.h"
#include "context/sim_snapshot.h"
#include "context/state.h"
#include "context/triple_buffer.h"
#include <atomic>
#include <cstdint>
#include <iostream>
#include <managers/cell_manager/cell_manager_settings.h>
#include <mutex>
#include <queue>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/WindowEnums.hpp>
#include <thread>


class Simulation : SimulationSettings
{
	SimulationToggles toggles_{};

	static sf::VideoMode getAdjustedVideoMode()
	{
		sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
		return sf::VideoMode({ (unsigned)(desktop.size.x * window_resize_shrinkage),
							   (unsigned)(desktop.size.y * window_resize_shrinkage) });
	}

	// Window mode
	bool         m_fullscreen_ = false;
	bool         m_titlebar_ = false;
	sf::Vector2u m_windowed_size_{};
	sf::Vector2i m_windowed_pos_{};

	// Handshake so the sim thread isn't touching the window while it's recreated
	std::atomic<bool> m_window_swap_{ false };
	std::atomic<bool> m_sim_parked_{ false };


	sf::VideoMode   videoMode = full_screen ? sf::VideoMode::getDesktopMode() : getAdjustedVideoMode();
	std::uint32_t   windowStyle = sf::Style::None;
	sf::RenderWindow m_window_{ videoMode, "Project ARIA", windowStyle };

	CustomClock<frame_smoothing> updating_clock_{ initial_frame_rate_updating };
	CustomClock<frame_smoothing> rendering_clock_{ initial_frame_rate_rendering };

	Camera camera_{ &m_window_, 1.f };

	World           m_world_{};
	ControlPanel    m_control_panel_;

	SimulationStatistics sim_state_{};

	bool  tracking = false;

	ImPlotColormap m_plot_colormap_{};

	// Multithreading
	int max = static_cast<int>(CellManagerSettings::max_cells);
	TripleBuffer<SimSnapshot> m_sim_buffer_{ max }; // sim -> render (lock-free)

	// render → sim  (low frequency, mutex protected)
	std::mutex             m_cmd_mutex{};
	std::queue<SimCommand> m_commands{};

	std::thread m_sim_thread_;
	std::atomic<bool> running{ true };


public:
	Simulation();
	void set_icon();
	~Simulation()
	{
		std::cout << "[INFO]: Simulation destructor called, shutting down threads and ImGui context.\n";
		if (m_sim_thread_.joinable()) {
			m_sim_thread_.join(); // Ensure the thread is joined before destruction
		}
	}
	void run_simulation();

	void check_end_of_sim_conditions();

	void print_end_of_sim_conditions();

private:
	void init_imGUI();
	void update_one_frame();
	void camera_follow_selected_protozoa();
	void handle_imGUI(const SimSnapshot& snapshot, float dt);
	void extinction_popup();
	void render();
	void manage_rendering_frame_rate();
	void manage_updating_frame_rate();
	void fill_snapshot(SimSnapshot& snapshot);
	void resolve_modifications();

	// window management
	void toggle_fullscreen();
	void toggle_titlebar();
	void recreate_window();

	// events
	void handle_events();
	bool try_select_protozoa(const sf::Vector2f& cam_pos);
	void handle_left_click(const sf::Vector2f& cam_pos);
	void handle_left_release();
	void handle_right_release();
	void handle_pause_toggle();
	void handle_keyboard_events(const sf::Keyboard::Key& event_key_code);
	void dispatch_event(const sf::Event& event, const sf::Vector2f& cam_pos);

	void handle_simulation_event(SimCommand& cmd);
};