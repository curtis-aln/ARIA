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
            const SimCommand& cmd = m_commands.front();
            switch (cmd.type)
            {
			case CommandType::SetToggles:
				m_world_.toggles = cmd.toggles;
				break;

            case CommandType::SetUpdatingFrameRate:
                sim_state_.max_frame_rate_updating = cmd.float_val;
				updating_clock_.set_target_fps(cmd.float_val);
				break;

            case CommandType::SetRenderingFrameRate:
                sim_state_.max_frame_rate_rendering = cmd.float_val;
                rendering_clock_.set_target_fps(cmd.float_val);
                break;

			case CommandType::ResetSimulation:
				m_world_.reset_world();
				break;

            //case CommandType::SetRadius:
            //    if (selected_protozoa)
            //        selected_protozoa->get_cells()[cmd.cell_spring_idx].radius = cmd.float_val;
            //    break;

            //case CommandType::SetAmplitude:
            //    if (selected_protozoa)
            //        selected_protozoa->get_cells()[cmd.cell_spring_idx].amplitude = cmd.float_val;
            //    break;

            case CommandType::SetFrequency:
                //if (selected_protozoa)
                //    selected_protozoa->get_cells()[cmd.cell_spring_idx].frequency = cmd.float_val;
                break;

            case CommandType::SetVerticalShift:
                //if (selected_protozoa)
                //    selected_protozoa->get_cells()[cmd.cell_spring_idx].vertical_shift = cmd.float_val;
                break;

            case CommandType::SetOffset:
                //if (selected_protozoa)
                //    selected_protozoa->get_cells()[cmd.cell_spring_idx].offset = cmd.float_val;
                break;

            case CommandType::SetCellGridResolution:
                //m_world_.get_spatial_grid()->change_cell_dimsensions(cmd.int_val, cmd.int_val);
                //m_world_.update_spatial_renderers();
                break;

            case CommandType::SetFoodGridResolution:
                //m_world_.get_food_spatial_grid()->change_cell_dimsensions(cmd.int_val, cmd.int_val);
                //m_world_.update_spatial_renderers();
                break;

            case CommandType::MutateProtozoa:
                //if (selected_protozoa)
               //     selected_protozoa->mutate(cmd.mutate.mut_rate, cmd.mutate.mut_range);
                break;

            case CommandType::AddCell:
                //if (selected_protozoa)
                //    selected_protozoa->add_cell();
                break;

            case CommandType::RemoveCell:
                //if (selected_protozoa)
                //    selected_protozoa->remove_cell();
                break;

            case CommandType::AddSpring:
                //if (selected_protozoa)
                //    selected_protozoa->add_spring(); todo
                break;

            case CommandType::RemoveSpring:
                //if (selected_protozoa)
                //    selected_protozoa->remove_spring(); todo
                break;

            case CommandType::InjectProtozoa:
                //if (selected_protozoa)
                //    m_world_.inject_protozoa(selected_protozoa, cmd.float_val);
                break;

            case CommandType::KillProtozoa:
                //if (selected_protozoa)
                //    selected_protozoa->kill(); // todo
                break;

            case CommandType::ForceReproduce:
                //if (selected_protozoa) // todo
                //    selected_protozoa->force_reproduce();
                break;

            case CommandType::MakeImmortal:
                //if (selected_protozoa) // todo
                //    selected_protozoa->immortal = cmd.bool_val;
                break;

            case CommandType::CloneProtozoa:
                //if (selected_protozoa)
                //{
                //    m_world_.create_offspring(selected_protozoa, false);
                //}
                break;

    
            case CommandType::NavToProtozoa:
                //m_world_.selected_protozoa_ = m_world_.all_protozoa_.at(cmd.int_val);
                break;

            case CommandType::SetSpringAmplitude:
                //if (selected_protozoa)
                //{
                //    m_world_.selected_protozoa_->get_springs()[cmd.cell_spring_idx].amplitude = cmd.float_val;
                //}
                break;

            case CommandType::SetSpringFrequency:
                //if (selected_protozoa)
                //{
                //    m_world_.selected_protozoa_->get_springs()[cmd.cell_spring_idx].frequency = cmd.float_val;
                //}
                break;

            case CommandType::SetSpringOffset:
                //if (selected_protozoa)
                //{
                //    m_world_.selected_protozoa_->get_springs()[cmd.cell_spring_idx].offset = cmd.float_val;
                //}
                //break;
                    //m_world_.selected_protozoa_->get_springs()[cmd.cell_spring_idx].offset = cmd.float_val;
                //}
                break;

            case CommandType::SetSpringVerticalShift:
                //if (selected_protozoa)
                //{
                    //m_world_.selected_protozoa_->get_springs()[cmd.cell_spring_idx].vertical_shift = cmd.float_val;
                //}
                break;

            case CommandType::SetDampingConst:
                //if (selected_protozoa)
                //{
                   // m_world_.selected_protozoa_->get_springs()[cmd.cell_spring_idx].damping = cmd.float_val;
                //}
                break;

            case CommandType::SetSpringConst:
                //if (selected_protozoa)
                //{
                   // m_world_.selected_protozoa_->get_springs()[cmd.cell_spring_idx].spring_const = cmd.float_val;
                //}
                break;
            }
            m_commands.pop();
        }

        const WorldToggles& t = m_world_.toggles; // or however you access live toggles

        auto* cell_manager = m_world_.get_cell_manager();
        WorldBorder bounds{ camera_.get_world_mouse_pos(), t.mouse_radius };

        if (right_mouse_pressed_event)
        {
            if (t.mouse_mode == 0) // Add
            {
                // adding cells
                if (t.mouse_add_cells)
                {
                    cell_manager->create_new_protozoa(1, &bounds);
                }

                if (t.mouse_add_food)
                {

                }
                //m_world_.food_manager.add_food_at_point(
                //	cam_pos,
                //	static_cast<int>(t.mouse_intensity),
                //	t.mouse_radius); // IMGUI_TODO: implement add_food_at_point()
            }
            else // Remove
            {
                if (t.mouse_rem_cells)
                {
                    std::cout << "removing cells";
                }
                //m_world_.get_cell_manager().remove_particles_in_radius(
                //	cam_pos, t.mouse_radius); // IMGUI_TODO: implement this

                if (t.mouse_rem_food)
                {

                }
                //m_world_.food_manager.remove_food_in_radius(
                //	cam_pos, t.mouse_radius); // IMGUI_TODO: implement this
            }
        }
    
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
        snapshot.stats.iterations_,
        snapshot.stats.cell_count,
        snapshot.stats.food_count,
        snapshot.stats.average_generation);

    m_history_.push_misc(
        snapshot.stats.average_mutation_rate,
        snapshot.stats.average_mutation_range,
        snapshot.stats.average_offspring_count,
        snapshot.stats.average_lifetime,
        snapshot.stats.average_cells_per_protozoa,
        snapshot.stats.average_spring_count,
        snapshot.stats.average_energy);
}

void Simulation::render()
{
    // Always grab the freshest completed simulation frame
    const SimSnapshot& snap = m_sim_buffer_.begin_read();
    float dt = static_cast<float>(rendering_clock_.get_delta_time());
    sim_state_.total_time_elapsed += dt;

    if (snap.stats.iterations_ <= 2)
        return;
    m_window_.clear(bg_color_);
    if (m_world_.toggles.m_rendering_)
    {
        const sf::Vector2f pos = camera_.get_world_mouse_pos();
        m_world_.render(snap, pos);
    }

    update_line_graphs(snap);
    handle_imGUI(snap, dt);

    m_sim_buffer_.end_read();

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

    snap.sim_state = sim_state_;
    snap.history = m_history_;
}