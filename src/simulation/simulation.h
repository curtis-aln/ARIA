#pragma once
#include "settings/settings.h"
#include "../world/world.h"
#include "imgui/population_history.h"
#include "imgui/control_panel.h"

#include "../Utils/custom_clock.h"
#include "../Utils/UI/Camera.hpp"

#include <imgui-SFML.h>
#include <implot.h>

#include "context/sim_snapshot.h"
#include "context/sim_command.h"
#include "context/triple_buffer.h"
#include <mutex>
#include <queue>
#include "../managers/cell_manager/organism_tracker.h"
#include "context/state.h"


class Simulation : SimulationSettings
{
	sf::Color bg_color_ = Random::rand_val_in_vector(bg_colors);
    static sf::VideoMode getAdjustedVideoMode()
    {
        sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
        return sf::VideoMode({ (unsigned)(desktop.size.x * resize_shrinkage),
                               (unsigned)(desktop.size.y * resize_shrinkage) });
    }

    sf::VideoMode   videoMode = full_screen ? sf::VideoMode::getDesktopMode() : getAdjustedVideoMode();
    std::uint32_t   windowStyle = sf::Style::None;
    sf::RenderWindow m_window_{ videoMode, "Project ARIA", windowStyle };

    CustomClock<frame_smoothing> updating_clock_{ initial_frame_rate_updating};
    CustomClock<frame_smoothing> rendering_clock_{ initial_frame_rate_rendering};

    Camera camera_{ &m_window_, 1.f };

    World           m_world_{};
    PopulationHistory m_history_;
    ControlPanel    m_control_panel_;

    SimulationStatistics sim_state_{};

    bool  tracking = false;
    
    bool  left_mouse_pressed_event = false;
    bool  right_mouse_pressed_event = false;

    ImPlotColormap m_plot_colormap_{};

    // Multithreading
    int max = static_cast<int>(CellManagerSettings::max_protozoa);
	TripleBuffer<SimSnapshot> m_sim_buffer_{ max }; // sim -> render (lock-free)

    // render → sim  (low frequency, mutex protected)
    std::mutex             m_cmd_mutex{};
    std::queue<SimCommand> m_commands{};

    std::thread m_sim_thread_;
    std::atomic<bool> running{ true };


public:
    Simulation();
    ~Simulation() 
	{
		std::cout << "[INFO]: Simulation destructor called, shutting down threads and ImGui context.\n";
        if (m_sim_thread_.joinable()) {
            m_sim_thread_.join(); // Ensure the thread is joined before destruction
        }
    }
    void run_simulation();

private:
    void init_imGUI();
    void update_one_frame();
    void camera_follow_selected_protozoa();
    void update_line_graphs(const SimSnapshot& snapshot);
    void handle_imGUI(const SimSnapshot & snapshot, float dt);
    void extinction_popup();
    void render();
    void manage_rendering_frame_rate();
    void manage_updating_frame_rate();
    void fill_snapshot(SimSnapshot& snapshot);
    void resolve_modifications();

    // events
    void handle_events();
    bool try_select_protozoa(const sf::Vector2f& cam_pos);
    void handle_left_click(const sf::Vector2f& cam_pos);
    void handle_right_click(const sf::Vector2f& cam_pos);
    void handle_left_release();
    void handle_right_release();
    void handle_pause_toggle();
    void handle_keyboard_events(const sf::Keyboard::Key& event_key_code);
    void dispatch_event(const sf::Event& event, const sf::Vector2f& cam_pos);
};