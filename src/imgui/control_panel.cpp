#include "control_panel.h"
#include "tabs/simulation_tab.h"
#include "tabs/statistics_tab.h"
#include "tabs/graphs_tab.h"
#include "tabs/organism_tab.h"
#include "tabs/tagged_tab.h"
#include "tabs/tools_tab.h"
#include "tabs/grid_tab.h"
#include "tabs/display_tab.h"
#include <imgui.h>

ControlPanel::ControlPanel()
{
    auto tagged = std::make_unique<TaggedTab>();
    m_tagged_tab_ = tagged.get();

    m_tabs_.push_back(std::make_unique<SimulationTab>());
    m_tabs_.push_back(std::make_unique<StatisticsTab>());
    m_tabs_.push_back(std::make_unique<GraphsTab>());
    m_tabs_.push_back(std::make_unique<OrganismTab>());
    m_tabs_.push_back(std::move(tagged));
    m_tabs_.push_back(std::make_unique<ToolsTab>());
    m_tabs_.push_back(std::make_unique<GridTab>());
    m_tabs_.push_back(std::make_unique<DisplayTab>());
}


void ControlPanel::draw(const SimSnapshot& snap, ImGuiContext& ctx, float dt)
{
    m_tagged_tab_->draw_toasts(dt);
    ImGui::SetNextWindowPos({ 10.f, 10.f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({ 520.f, 640.f }, ImGuiCond_FirstUseEver);

	ImGui::Begin("ARIA Control Panel", nullptr, ImGuiWindowFlags_NoNav);

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