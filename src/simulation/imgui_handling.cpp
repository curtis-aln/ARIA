#include "simulation.h"


void Simulation::init_imGUI()
{
    if (!ImGui::SFML::Init(m_window_))
        std::cerr << "[ERROR]: Failed to initialize ImGui-SFML\n";

    ImGui::GetIO().FontGlobalScale = ui_scale_percent / 100.f;

    ImPlot::CreateContext();
}


void Simulation::handle_imGUI(const SimSnapshot& snap, float dt)
{
    sf::Time delta_time = sf::seconds(static_cast<float>(dt));
    ImGui::SFML::Update(m_window_, delta_time);

    WorldToggles toggles_copy = snap.toggles;
    ImGuiContext ctx{ toggles_copy, m_cmd_mutex, m_commands };

    m_control_panel_.draw(snap, ctx, dt);

    if (std::memcmp(&toggles_copy, &snap.toggles, sizeof(WorldToggles)) != 0)
        ctx.push({ CommandType::SetToggles, toggles_copy });

    if (toggles_copy.open_extinction_window)
        ImGui::OpenPopup("New Simulation");
}


void Simulation::extinction_popup()
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

        step_1 = 1000;
        step_fast = 10'000;
        ImGui::InputFloat("World radius", &world_radius, step_1, step_fast);
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