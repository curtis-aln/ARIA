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
}

void Simulation::run_simulation()
{

    m_sim_thread_ = std::thread([this]
    {
            Random::set_seed(0);
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
    if (m_world_.toggles.m_tick_frame_time)
    {
        update_world();
        m_world_.toggles.m_tick_frame_time = false;
    }
    else if (!m_world_.toggles.paused)
    {
        update_world();
                    
    }

    if (m_total_time_elapsed_ >= 30)
    {
        //running = false;
        //std::cout << m_total_time_elapsed_ << " time elapsed \n";
		//std::cout << m_world_.get_statistics().iterations_ << " iterations \n";
        //std::cout << m_world_.get_statistics().entity_count << " entity count \n";
    }

    camera_follow_selected_protozoa();
    
}

void Simulation::update_world()
{
    // Apply any commands ImGui pushed since last tick
    {
		// lock the command queue while we process it, but release it before ticking the simulation
        std::lock_guard lock(m_cmd_mutex);
		Protozoa* selected_protozoa = m_world_.get_selected_protozoa();

        while (!m_commands.empty())
        {
            const SimCommand& cmd = m_commands.front();
            switch (cmd.type)
            {
            case CommandType::SetToggles:
                m_world_.toggles = cmd.toggles;
                break;

            case CommandType::SetRadius:
                if (selected_protozoa)
                    selected_protozoa->get_cells()[cmd.cell_spring_idx].radius = cmd.float_val;
                break;

			case CommandType::SetAmplitude:
				if (selected_protozoa)
					selected_protozoa->get_cells()[cmd.cell_spring_idx].amplitude = cmd.float_val;
				break;

            case CommandType::SetFrequency:
				if (selected_protozoa)
					selected_protozoa->get_cells()[cmd.cell_spring_idx].frequency = cmd.float_val;
				break;

            case CommandType::SetVerticalShift:
                if (selected_protozoa)
                    selected_protozoa->get_cells()[cmd.cell_spring_idx].vertical_shift = cmd.float_val;
                break;

            case CommandType::SetOffset:
                if (selected_protozoa)
                    selected_protozoa->get_cells()[cmd.cell_spring_idx].offset = cmd.float_val;
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
				if (selected_protozoa)
					selected_protozoa->mutate(cmd.mutate.mut_rate, cmd.mutate.mut_range);
                break;

            case CommandType::AddCell:
				if (selected_protozoa)
					selected_protozoa->add_cell();
				break;  

            case CommandType::RemoveCell:
                if (selected_protozoa)
                    selected_protozoa->remove_cell();
                break;

            case CommandType::AddSpring:
                if (selected_protozoa)
                    selected_protozoa->add_spring();
                break;

            case CommandType::RemoveSpring:
                if (selected_protozoa)
                    selected_protozoa->remove_spring();
                break;

            case CommandType::InjectProtozoa:
                if (selected_protozoa)
                    selected_protozoa->inject(cmd.float_val);
                break;

            case CommandType::KillProtozoa:
                if (selected_protozoa)
                    selected_protozoa->kill();
                break;

            case CommandType::ForceReproduce:
                if (selected_protozoa)
                    selected_protozoa->force_reproduce();
                break;

            case CommandType::MakeImmortal:
				if (selected_protozoa)
					selected_protozoa->immortal = cmd.bool_val;
				break;

            case CommandType::CloneProtozoa:
                if (selected_protozoa)
                {
					m_world_.create_offspring(selected_protozoa, false);
                }
                break;

            case CommandType::ResetSimulation:
                break; // todo

            case CommandType::NavToProtozoa:
                m_world_.selected_protozoa_ = m_world_.all_protozoa_.at(cmd.int_val);

            }
            m_commands.pop();
        }
    } // mutex released here

    // Tick the simulation
    m_world_.update();
    
    // Package results into the triple buffer
    SimSnapshot& snap = m_sim_buffer_.get_write_buffer();

    // Filling the snapshot with information
    m_world_.fill_snapshot(snap);
	snap.stats.fps = fps_;
	snap.stats.m_total_time_elapsed_ = m_total_time_elapsed_;
    snap.history = m_history_;
    
    m_sim_buffer_.publish();


}

void Simulation::camera_follow_selected_protozoa()
{
    Protozoa* selected = m_world_.get_selected_protozoa();
    if (selected == nullptr)
        return;

    const sf::Rect<float> bounds = m_world_.calc_protozoa_bounds(selected);
    const sf::Vector2f    target = bounds.position + bounds.size / 2.f;
    const sf::Vector2f    current = camera_.m_view_.getCenter();

    camera_.m_view_.setCenter(current + (target - current) * camera_lerp_factor);
    camera_.update_window_view();
}

void Simulation::update_line_graphs(const SimSnapshot& snapshot)
{
    m_history_.push(
        snapshot.stats.iterations_,
        snapshot.stats.protozoa_count,
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

    if (snap.stats.iterations_ <= 1)
        return;
    m_window_.clear(GraphicalSettings::window_color);
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
    fps_ = static_cast<float>(m_clock_.get_average_frame_rate());
    m_clock_.update_frame_rate();

    const WorldStatistics& stats = m_world_.get_statistics();

    std::ostringstream title;
    title << simulation_name
        << " | FPS: " << std::fixed << std::setprecision(1) << fps_
        << " | protozoa: " << stats.protozoa_count
        << " | food: " << m_world_.get_food_count()
        << " | average generation: " << stats.average_generation;
    m_window_.setTitle(title.str());
}

void Simulation::fill_snapshot(SimSnapshot& snapshot)
{
	m_world_.fill_snapshot(snapshot);
    snapshot.stats.m_total_time_elapsed_ = m_total_time_elapsed_;
	snapshot.stats.fps = fps_;
  
}