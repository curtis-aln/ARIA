#include "control_panel.h"
#include "tabs/simulation_tab.h"
#include "tabs/graphs_tab.h"
#include "tabs/organism_tab.h"
#include "tabs/tagged_tab.h"
#include "tabs/grid_tab.h"
#include "tabs/o_vector_tab.h"
#include <imgui.h>

#include "imgui_settings.h"

// This contains all of the tabs for the control panel, 
// and is responsible for drawing the window and the tab bar.  Each tab is responsible for drawing its own contents.

ControlPanel::ControlPanel()
{
    auto tagged = std::make_unique<TaggedTab>();
    m_tagged_tab_ = tagged.get();

	// The order of tabs here is the order they will appear in the control panel.
    m_tabs_.push_back(std::make_unique<SimulationTab>());
    m_tabs_.push_back(std::make_unique<GraphsTab>());
    m_tabs_.push_back(std::make_unique<OrganismTab>());
    m_tabs_.push_back(std::move(tagged));
    m_tabs_.push_back(std::make_unique<GridTab>());
    m_tabs_.push_back(std::make_unique<OVecDebugTab>());
}


void ControlPanel::draw(const SimSnapshot& snap, ImGuiContext& ctx, float dt)
{
    m_tagged_tab_->draw_toasts(dt);
    ImGui::SetNextWindowPos({ 10.f, 10.f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({ 520.f, 640.f }, ImGuiCond_FirstUseEver);

	ImGui::Begin(control_panel_window_name, nullptr, ImGuiWindowFlags_NoNav);

	if (ImGui::BeginTabBar("##ctrl_tabs"))
    {
        for (auto& tab : m_tabs_)
        {
            if (ImGui::BeginTabItem(tab->label()))
            {
                tab->draw(snap, ctx);
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}