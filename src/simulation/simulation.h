#pragma once
#include "../settings.h"
#include "../world/world.h"
#include "imgui/population_history.h"
#include "imgui/control_panel.h"

#include "../Utils/time.h"
#include "../Utils/fps_manager.h"
#include "../Utils/UI/Camera.hpp"

#include <imgui-SFML.h>
#include <implot.h>

#include "sim_snapshot.h"
#include "triple_buffer.h"
#include "sim_command.h"
#include <mutex>
#include <queue>
#include "../world/protozoa_tracker.h"



class Simulation : SimulationSettings, TextSettings
{
    static sf::VideoMode getAdjustedVideoMode()
    {
        sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
        return sf::VideoMode({ (unsigned)(desktop.size.x * resize_shrinkage),
                               (unsigned)(desktop.size.y * resize_shrinkage) });
    }

    Font title_font{ nullptr, title_font_size, bold_font_location };
    Font regular_font{ nullptr, regular_font_size, regular_font_location };
    Font cell_statistic_font{ nullptr, cell_statistic_font_size, regular_font_location };

    sf::VideoMode   videoMode = full_screen ? sf::VideoMode::getDesktopMode() : getAdjustedVideoMode();
    std::uint32_t   windowStyle = sf::Style::None;
    sf::RenderWindow m_window_{ videoMode, "Project ARIA", windowStyle };

    FrameRateSmoothing<frame_smoothing> m_clock_{};
    Camera camera_{ &m_window_, 1.f };

    StopWatch m_delta_time_{};
    float    m_total_time_elapsed_ = 0.f;

    World           m_world_{};
    PopulationHistory m_history_;
    ControlPanel    m_control_panel_;

    bool  tracking = false;
    
    bool  mouse_pressed_event = false;
    float fps_ = 0.f;

    ImPlotColormap m_plot_colormap_{};

    // Multithreading
    int max = static_cast<int>(WorldSettings::max_protozoa * ProtozoaSettings::max_cells);
	TripleBuffer<SimSnapshot> m_sim_buffer_{ max }; // sim -> render (lock-free)

    // render → sim  (low frequency, mutex protected)
    std::mutex             m_cmd_mutex{};
    std::queue<SimCommand> m_commands{};

    std::thread m_sim_thread_;
    std::atomic<bool> running{ true }; // replace your existing bool if you have one


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
    void manage_frame_rate();
    void fill_snapshot(SimSnapshot& snapshot);
    void update_world();

    // events
    void handle_events();
    bool try_select_protozoa(const sf::Vector2f& cam_pos);
    void handle_mouse_press(const sf::Vector2f& cam_pos);
    void handle_mouse_release();
    void handle_pause_toggle();
    void handle_keyboard_events(const sf::Keyboard::Key& event_key_code);
    void dispatch_event(const sf::Event& event, const sf::Vector2f& cam_pos);
};