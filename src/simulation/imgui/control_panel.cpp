#include "control_panel.h"
#include "tabs/debug_tab.h"
#include "tabs/food_tab.h"
#include "tabs/graphs/graphs_tab.h"
#include "tabs/grid_tab.h"
#include "tabs/o_vector/o_vector_tab.h"
#include "tabs/organism/organism_tab.h"
#include "tabs/simulation_tab.h"
#include <cstring>
#include <imgui.h>

#include "imgui_settings.h"
#include <memory>
#include <simulation/context/sim_command.h>
#include <simulation/context/sim_snapshot.h>

ControlPanel::ControlPanel()
{
	m_tabs_.push_back(std::make_unique<SimulationTab>());
	m_tabs_.push_back(std::make_unique<GraphsTab>());
	m_tabs_.push_back(std::make_unique<OrganismTab>());
	m_tabs_.push_back(std::make_unique<GridTab>());
	m_tabs_.push_back(std::make_unique<OVecDebugTab>());
	m_tabs_.push_back(std::make_unique<DebugTab>());
	m_tabs_.push_back(std::make_unique<FoodTab>());
}

void ControlPanel::select_tab(const char* label)
{
	m_pending_tab_label_ = label;
}

void ControlPanel::draw(const SimSnapshot& snap, ImGuiContext& ctx, float dt)
{
	constexpr float PANEL_W = 1520.f;
	constexpr float PANEL_H = 340.f;

	ImGui::SetNextWindowPos({ 10.f, 10.f }, ImGuiCond_Always);
	ImGui::SetNextWindowSize({ PANEL_W, PANEL_H }, ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.94f);

	constexpr ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoResize |          // user can't drag the window edges to resize it
		ImGuiWindowFlags_NoMove |            // user can't drag the window around
		ImGuiWindowFlags_NoScrollbar |       // window itself never shows a scrollbar (child panels still can)
		//ImGuiWindowFlags_NoScrollWithMouse |  // (disabled) would stop the mouse wheel scrolling the window
		ImGuiWindowFlags_NoTitleBar |        // hides the title bar (you draw your own title text instead)
		ImGuiWindowFlags_NoSavedSettings;    // don't save/restore position and size in imgui.ini


	ImGui::Begin("##ARIACtrl", nullptr, flags);

	// Title bar
	ImGui::PushStyleColor(ImGuiCol_Text, { 0.72f, 0.84f, 1.00f, 1.f });
	ImGui::Text(control_panel_window_name);
	ImGui::PopStyleColor();

	ImGui::Spacing();

	if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_DrawSelectedOverline))
	{
		for (auto& tab : m_tabs_)
		{
			ImGuiTabItemFlags tab_flags = 0;
			if (m_pending_tab_label_ && std::strcmp(tab->label(), m_pending_tab_label_) == 0)
				tab_flags |= ImGuiTabItemFlags_SetSelected;

			if (ImGui::BeginTabItem(tab->label(), nullptr, tab_flags))
			{
				ImGui::Spacing();
				tab->draw(snap, ctx);
				ImGui::Spacing();
				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();
	}

	m_pending_tab_label_ = nullptr; // one-shot: consume the request

	ImGui::End();
}