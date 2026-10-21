#include "context/sim_command.h"
#include "context/sim_snapshot.h"
#include "settings/settings.h"
#include "simulation.h"
#include <imgui-SFML.h>
#include <implot.h>
#include <iostream>
#include <mutex>
#include <ostream>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Mouse.hpp>
#include <thread>
#include <Utils/random.h>
#include <world/world_border.h>
#include <world/world_settings.h>


Simulation::Simulation() : m_world_(&m_window_)
{
	window_color = Random::rand_val_in_vector(bg_colors);

	m_window_.setVerticalSyncEnabled(vsync);

	set_icon();


	const float rad = WorldSettings::bounds_radius;
	camera_.set_initial_zoom(0.1f, sf::Vector2f{ rad, rad });

	init_imGUI();

	// The simulation is designed to run at 30 iterations per second
	sim_state_.max_frame_rate_updating = SimulationSettings::initial_frame_rate_updating;
	sim_state_.max_frame_rate_rendering = SimulationSettings::initial_frame_rate_rendering;
	updating_clock_.set_target_fps(sim_state_.max_frame_rate_updating);
	rendering_clock_.set_target_fps(sim_state_.max_frame_rate_rendering);

	std::cout << "Program Finished initialising. running. . .\n";
}

void Simulation::set_icon()
{
	sf::Image icon;
	if (icon.loadFromFile("media/icon.png"))
	{
		sf::Texture tex;
		tex.loadFromImage(icon);
		tex.setSmooth(true);                      // bilinear filtering, avoids blocky pixels

		const float zoom = 2.8f;
		sf::Vector2u size = icon.getSize();

		// SFML 3:
		sf::RenderTexture rt(size);
		sf::Sprite sprite(tex);
		sprite.setOrigin({ size.x / 2.f, size.y / 2.f });
		sprite.setPosition({ size.x / 2.f, size.y / 2.f });
		sprite.setScale({ zoom, zoom });

		rt.clear(sf::Color::Transparent);
		rt.draw(sprite);
		rt.display();

		sf::Image zoomed = rt.getTexture().copyToImage();
		m_window_.setIcon(zoomed.getSize(), zoomed.getPixelsPtr());   // SFML 2: (x, y, ptr)
	}
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

		check_end_of_sim_conditions();
	}

	print_end_of_sim_conditions();

	m_sim_thread_.join();
	ImGui::SFML::Shutdown();
	ImPlot::DestroyContext();
}

void Simulation::check_end_of_sim_conditions()
{
	// If a maximum simulation time is set (non-zero), check if the total elapsed time has exceeded it. If so, stop the simulation.
	bool time_stop_condition = (max_simulation_time != 0.f && sim_state_.total_time_elapsed > max_simulation_time);
	bool frame_stop_condition = (max_iterations != 0 && m_world_.get_statistics().iterations_ >= max_iterations);
	if (time_stop_condition || frame_stop_condition)
		running = false;
}

void Simulation::print_end_of_sim_conditions()
{
	std::cout << "End of Sim Statistics\n";
	std::cout << "total time elapsed: " << sim_state_.total_time_elapsed << " seconds" << std::endl;
	std::cout << "total iterations: " << m_world_.get_statistics().iterations_ << std::endl;
	std::cout << "Cells " << m_world_.get_cell_count() << std::endl;
	std::cout << "Food " << m_world_.get_food_count() << std::endl;
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

	if (m_world_.get_cell_manager()->extinction_event)
	{
		running = false;
	}
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
				m_world_.get_food_manager()->handle_food_manager_event(cmd);
				break;
			}

			m_commands.pop();
		}


		const auto& stats = m_world_.get_statistics();
		const auto t = m_world_.world_toggles;

		auto* cell_manager = m_world_.get_cell_manager();
		WorldBorder bounds{ camera_.get_world_mouse_pos(), m_world_.get_statistics().mouse_radius };

		if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Right))
			m_world_.handle_right_click(bounds);
	} // mutex released here
}




void Simulation::camera_follow_selected_protozoa()
{
	const sf::Vector2f* pos = m_world_.get_cell_manager()->get_selected_protozoa_pos();
	if (pos == nullptr || m_world_.dragging) // No protozoa is selected, so we don't need to move the camera
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

	m_window_.clear(window_color);
	if (toggles_.m_rendering_)
	{
		const sf::Vector2f pos = camera_.get_world_mouse_pos();
		m_world_.render(snap, pos);
	}

	handle_imGUI(snap, dt);

	m_sim_buffer_.end_read();

	if (!toggles_.hide_panels)
		ImGui::SFML::Render(m_window_);
	m_window_.display();
}

void Simulation::manage_rendering_frame_rate()
{
	rendering_clock_.tick();
	rendering_clock_.limit_frame_rate();

	sim_state_.rendering_frame_rate = (float)rendering_clock_.get_average_frame_rate();
}

void Simulation::manage_updating_frame_rate()
{
	// This is how the frame rate is limited to the desired fps
	updating_clock_.tick();
	updating_clock_.limit_frame_rate();

	sim_state_.updating_frame_rate = (float)updating_clock_.get_average_frame_rate();
}

void Simulation::fill_snapshot(SimSnapshot& snap)
{
	snap.sim_toggles = toggles_;
	sim_state_.camera_zoom = camera_.get_current_zoom();

	sf::Vector2f mouse_pos = camera_.get_world_mouse_pos();
	sim_state_.mouse_pos_x = mouse_pos.x;
	sim_state_.mouse_pos_y = mouse_pos.y;

	snap.sim_stats = sim_state_;
	snap.history = m_history_;
}