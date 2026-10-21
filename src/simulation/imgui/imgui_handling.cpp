#include "../simulation.h"
#include "theme.h"
#include <imgui.h>
#include <imgui-SFML.h>
#include <implot.h>
#include <iostream>
#include <SFML/System/Time.hpp>
#include <simulation/context/sim_command.h>
#include <simulation/context/sim_snapshot.h>
#include <simulation/context/state.h>
#include <string.h>

void Simulation::init_imGUI()
{
	if (!ImGui::SFML::Init(m_window_))
		std::cerr << "[ERROR]: Failed to initialize ImGui-SFML\n";

	apply_aria_theme();

	ImGui::GetIO().FontGlobalScale = ui_scale_percent / 100.f;

	ImPlot::CreateContext();
	apply_aria_implot_theme();
	std::cout << "ImGUI initialised\n";
}

template <typename ToggleT>
void push_if_changed(ImGuiContext& ctx, const ToggleT& copy, const ToggleT& original,
	CommandSection section, CommandType type, ToggleT SimCommand::* cmd_field)
{
	if (std::memcmp(&copy, &original, sizeof(ToggleT)) != 0)
	{
		SimCommand cmd{ .section = section, .type = type };
		cmd.*cmd_field = copy;
		ctx.push(cmd);
	}
}


void Simulation::handle_imGUI(const SimSnapshot& snap, float dt)
{
	sf::Time delta_time = sf::seconds(static_cast<float>(dt));
	ImGui::SFML::Update(m_window_, delta_time);

	SimulationToggles  sim_toggles_copy = snap.sim_toggles;
	WorldToggles world_toggles_copy = snap.world_toggles;
	FoodToggles food_toggles_copy = snap.food_toggles;
	CellManagerToggles cell_toggles_copy = snap.cell_toggles;
	ImGuiContext ctx{ sim_toggles_copy, cell_toggles_copy, world_toggles_copy, food_toggles_copy, m_cmd_mutex, m_commands };

	m_control_panel_.draw(snap, ctx, dt);

	push_if_changed(ctx, world_toggles_copy, snap.world_toggles, CommandSection::WorldEvent, CommandType::SetWorldToggles, &SimCommand::world_toggles);
	push_if_changed(ctx, cell_toggles_copy, snap.cell_toggles, CommandSection::CellManagerEvent, CommandType::SetCellToggles, &SimCommand::cell_toggles);
	push_if_changed(ctx, food_toggles_copy, snap.food_toggles, CommandSection::FoodManagerEvent, CommandType::SetFoodToggles, &SimCommand::food_toggles);
	push_if_changed(ctx, sim_toggles_copy, snap.sim_toggles, CommandSection::SimulationEvent, CommandType::SetSimToggles, &SimCommand::sim_toggles);
}


void Simulation::extinction_popup() // TODO: depricated
{
	if (ImGui::BeginPopupModal("New Simulation", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		// Center the popup
		ImGui::SetWindowPos(
			ImVec2(
				(ImGui::GetIO().DisplaySize.x - ImGui::GetWindowSize().x) * 0.5f,
				(ImGui::GetIO().DisplaySize.y - ImGui::GetWindowSize().y) * 0.5f
			)
		);

		static int starting_food = 1000;
		static int starting_protozoa = 50;
		static int max_food = 2000;
		static int max_protozoa = 200;
		static float world_radius = 10'000;

		static float mutation_rate = 0.01f;
		static float mutation_range = 0.1f;
		static float food_spawn_rate = 0.5f;

		ImGui::SeparatorText("Simulation Settings");

		int step_1 = 100;
		int step_fast = 1000;
		ImGui::InputInt("Starting Food", &starting_food, step_1, step_fast);
		ImGui::InputInt("Starting Protozoa", &starting_protozoa, step_1, step_fast);
		ImGui::InputInt("Max Food", &max_food, step_1, step_fast);
		ImGui::InputInt("Max Protozoa", &max_protozoa, step_1, step_fast);

		float step_2 = 1000.f;
		float step_fast_2 = 10'000.f;
		ImGui::InputFloat("World radius", &world_radius, step_2, step_fast_2);
#
		ImGui::Spacing();

		ImGui::SeparatorText("Evolution Settings");

		ImGui::InputFloat("Mutation Rate", &mutation_rate, 0.001f, 0.01f, "%.4f");
		ImGui::InputFloat("Mutation Range", &mutation_range, 0.01f, 0.1f, "%.4f");
		ImGui::InputFloat("Food Spawn Rate", &food_spawn_rate, 0.01f, 1.0f, "%.2f");

		ImGui::Spacing();

		if (ImGui::Button("Start"))
		{

			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();

		if (ImGui::Button("Cancel"))
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}