#include "simulation.h"
#include <implot.h>

Simulation::Simulation() : m_world_(&m_window_)
{
    m_window_.setFramerateLimit(max_fps);
    m_window_.setVerticalSyncEnabled(vsync);

    const float rad = WorldSettings::bounds_radius;
    camera_.m_view_.setCenter({ rad, rad });
    camera_.update_window_view();

    init_imGUI();

    // The simulation is designed to run at 30 iterations per second
    sim_state_.max_frame_rate_updating = SimulationSettings::initial_frame_rate_updating;
    sim_state_.max_frame_rate_rendering = SimulationSettings::initial_frame_rate_rendering;
    updating_clock_.set_target_fps(sim_state_.max_frame_rate_updating);
    rendering_clock_.set_target_fps(sim_state_.max_frame_rate_rendering);

    std::cout << "Program Finished initialising. running. . .\n";
}

void Simulation::run_simulation()
{
  
    update_one_frame(); // a lot of issues happen in the renderer when it runs before the very first update iter
    m_sim_thread_ = std::thread([this]
    {
            while (running)
                update_one_frame();
        });

    while (running)
    {
        handle_events();
        manage_rendering_frame_rate();
        render();

        if (sim_state_.total_time_elapsed > 60)
        {
			std::cout << "total iterations: " << m_world_.get_statistics().iterations_ << std::endl;
            running = false;
        }
    }

    m_sim_thread_.join();
    ImGui::SFML::Shutdown();
    ImPlot::DestroyContext();
}

void Simulation::update_one_frame()
{
    manage_updating_frame_rate();

    // Any interputs made by imgui need to be processed by the updating thread
    resolve_modifications();

	// Retrieve a writable snapshot from the triple buffer. This snapshot will be filled with the current state of the simulation.
    SimSnapshot& snap = m_sim_buffer_.get_write_buffer();
  
    m_world_.update(snap); 
    fill_snapshot(snap);
    update_line_graphs(snap);

	m_sim_buffer_.publish(); // Publish the filled snapshot to make it available for rendering.

    camera_follow_selected_protozoa(); 
}


void Simulation::resolve_modifications()
{
    // Apply any commands ImGui pushed since last tick
    {
        // lock the command queue while we process it, but release it before ticking the simulation
        std::lock_guard lock(m_cmd_mutex);
        //Protozoa* selected_protozoa = m_world_.get_selected_protozoa();

        while (!m_commands.empty())
        {
            SimCommand& cmd = m_commands.front();
            switch (cmd.section)
            {
			case CommandSection::SimulationEvent:
				// Handle simulation-related commands
				handle_simulation_event(cmd);
				break;

			case CommandSection::WorldEvent:
				// Handle world-related command
				m_world_.handle_world_event(cmd);
				break;

			case CommandSection::CellManagerEvent:
				// Handle cell manager-related commands
				m_world_.get_cell_manager()->handle_cell_manager_event(cmd);
				break;

			case CommandSection::FoodManagerEvent:
				// Handle food manager-related commands
				break;
            }

            m_commands.pop();
        }

    
        const auto& stats = m_world_.get_statistics();
        const auto t = m_world_.toggles;

        auto* cell_manager = m_world_.get_cell_manager();
        WorldBorder bounds{ camera_.get_world_mouse_pos(), m_world_.get_statistics().mouse_radius };

        if (right_mouse_pressed_event)
            m_world_.handle_right_click(bounds);
    } // mutex released here
}




void Simulation::camera_follow_selected_protozoa()
{
	const sf::Vector2f* pos = m_world_.get_cell_manager()->get_selected_protozoa_pos();
	if (pos == nullptr || m_world_.should_drag_protozoa_) // No protozoa is selected, so we don't need to move the camera
		return;

	const sf::Vector2f cam_pos = camera_.m_view_.getCenter();
    camera_.m_view_.setCenter(cam_pos + (*pos - cam_pos) * camera_lerp_factor);
    camera_.update_window_view();
}

void Simulation::update_line_graphs(const SimSnapshot& snapshot)
{
    m_history_.push(
        snapshot.cell_manager_stats.cell_count,
        snapshot.food_manager_stats.food_count,
        snapshot.cell_manager_stats.average_generation,
        snapshot.cell_manager_stats.average_mutation_rate,
        snapshot.cell_manager_stats.average_mutation_range,
        snapshot.cell_manager_stats.average_offspring_count,
        snapshot.cell_manager_stats.average_lifetime,
        snapshot.cell_manager_stats.average_cells_per_protozoa,
        snapshot.cell_manager_stats.average_spring_count,
        snapshot.cell_manager_stats.average_energy);
}

void Simulation::render()
{
    // Always grab the freshest completed simulation frame
    const SimSnapshot& snap = m_sim_buffer_.begin_read();
    float dt = static_cast<float>(rendering_clock_.get_delta_time());
    sim_state_.total_time_elapsed += dt;

    if (snap.world_stats.iterations_ <= 2)
        return;
    m_window_.clear(bg_color_);
    if (m_world_.toggles.m_rendering_)
    {
        const sf::Vector2f pos = camera_.get_world_mouse_pos();
        m_world_.render(snap, pos);
    }

    handle_imGUI(snap, dt);

    m_sim_buffer_.end_read();

    if (!m_world_.toggles.hide_panels)
        ImGui::SFML::Render(m_window_);
    m_window_.display();
}

void Simulation::manage_rendering_frame_rate()
{
    rendering_clock_.tick();
    rendering_clock_.limit_frame_rate();

    sim_state_.rendering_frame_rate = rendering_clock_.get_average_frame_rate();
}

void Simulation::manage_updating_frame_rate()
{
    // This is how the frame rate is limited to the desired fps
    updating_clock_.tick();
	updating_clock_.limit_frame_rate();

    sim_state_.updating_frame_rate = updating_clock_.get_average_frame_rate();
}

void Simulation::fill_snapshot(SimSnapshot& snap)
{
    sim_state_.camera_zoom = camera_.get_current_zoom();
    
	sf::Vector2f mouse_pos = camera_.get_world_mouse_pos();
	sim_state_.mouse_pos_x = mouse_pos.x;
	sim_state_.mouse_pos_y = mouse_pos.y;

    snap.sim_stats = sim_state_;
    snap.history = m_history_;
}