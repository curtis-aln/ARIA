#include "simulation.h"
#include <implot.h>

Simulation::Simulation() : m_world_(&m_window_)
{
    m_window_.setFramerateLimit(max_fps);
    m_window_.setVerticalSyncEnabled(vsync);

    const float rad = WorldSettings::bounds_radius;
    camera_.m_view_.setCenter({ rad, rad });
    camera_.update_window_view();

    title_font.set_render_window(&m_window_);
    regular_font.set_render_window(&m_window_);
    cell_statistic_font.set_render_window(&m_window_);

    title_font.set_font_size(title_font_size);
    regular_font.set_font_size(regular_font_size);
    cell_statistic_font.set_font_size(cell_statistic_font_size);

    init_imGUI();
    std::cout << "Program Finished initialising. running. . .\n";
}

void Simulation::run_simulation()
{
    // The simulation is designed to run at 30 iterations per second
    m_world_.toggles.max_frame_rate = SimulationSettings::initial_frame_rate;

    update_one_frame(); // a lot of issues happen in the renderer when it runs before the very first update iter
    m_sim_thread_ = std::thread([this]
    {
            while (running)
                update_one_frame();
        });

    while (running)
    {
        handle_events();
        manage_frame_rate();
        render();
    }

    m_sim_thread_.join();
    ImGui::SFML::Shutdown();
    ImPlot::DestroyContext();
}

void Simulation::update_one_frame()
{
    // This is how the frame rate is limited to the desired fps
    if (m_world_.toggles.max_frame_rate != 0)
        m_frame_rater_.sleep();
   
    // Any interputs made by imgui need to be processed by the updating thread
    resolve_modifications();

	// Retrieve a writable snapshot from the triple buffer. This snapshot will be filled with the current state of the simulation.
    SimSnapshot& snap = m_sim_buffer_.get_write_buffer();
  
    m_world_.update(snap); 
    fill_snapshot(snap);

	m_sim_buffer_.publish(); // Publish the filled snapshot to make it available for rendering.

    if (m_total_time_elapsed_ >= 30)
    {
        //running = false;
        //std::cout << m_total_time_elapsed_ << " time elapsed \n";
		//std::cout << m_world_.get_statistics().iterations_ << " iterations \n";
        //std::cout << m_world_.get_statistics().entity_count << " entity count \n";
    }

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

			case CommandType::SetFrameRate:
                m_world_.toggles.max_frame_rate = cmd.float_val;
				m_frame_rater_.set_fps(cmd.float_val);
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

            case CommandType::ResetSimulation:
                break; // todo

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
    float dt = static_cast<float>(m_delta_time_.get_delta());
    m_total_time_elapsed_ += dt;

    if (snap.stats.iterations_ <= 2)
        return;
    m_window_.clear(bg_color_);
    if (m_world_.toggles.m_rendering_)
    {
        const sf::Vector2f pos = camera_.get_world_mouse_pos();
        m_world_.render(snap, &cell_statistic_font, pos);
    }

    update_line_graphs(snap);
    handle_imGUI(snap, dt);

    m_sim_buffer_.end_read();

    

    ImGui::SFML::Render(m_window_);
    m_window_.display();
}

void Simulation::manage_frame_rate()
{
    rendering_frame_rate = static_cast<float>(m_clock_.get_average_frame_rate());
    m_clock_.update_frame_rate();

    const WorldStatistics& stats = m_world_.get_statistics();

    std::ostringstream title;
    title << simulation_name
        << " | FPS: " << std::fixed << std::setprecision(1) << rendering_frame_rate
        << " | protozoa: " << stats.cell_count
        << " | food: " << m_world_.get_food_count()
        << " | average generation: " << stats.average_generation;
    m_window_.setTitle(title.str());
}

void Simulation::fill_snapshot(SimSnapshot& snap)
{
    snap.stats.m_total_time_elapsed_ = m_total_time_elapsed_;
	snap.stats.rendering_frame_rate = rendering_frame_rate;

    snap.history = m_history_;
    snap.render.camera_zoom = camera_.get_current_zoom();
}