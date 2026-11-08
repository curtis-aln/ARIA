#include "control_panel.h"
#include "../tabs/debug/debug_tab.h"
#include "../tabs/food/food_tab.h"
#include "../tabs/graphs/graphs_tab.h"
#include "../tabs/grid/grid_tab.h"
#include "../tabs/o_vector/o_vector_tab.h"
#include "../tabs/organism/organism_tab.h"
#include "../tabs/sim/simulation_tab.h"
#include <algorithm>
#include <cstring>
#include <imgui.h>

#include "../misc/imgui_settings.h"
#include <memory>
#include <simulation/context/sim_command.h>
#include <simulation/context/sim_snapshot.h>
#include <utility>

namespace
{
	constexpr float k_popout_min_w = 800.f;
	constexpr float k_popout_min_h = 300.f;
}

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

ControlPanel::~ControlPanel()
{
	close_popout();
}

void ControlPanel::set_main_window(sf::RenderWindow& main)
{
	m_main_ = &main;
}

void ControlPanel::set_popout_init(std::function<void()> fn)
{
	m_popout_init_ = std::move(fn);
}

void ControlPanel::close_popout()
{
	m_popout_.close();
}

void ControlPanel::select_tab(const char* label)
{
	m_pending_tab_label_ = label;
	m_collapsed_ = false;
	m_visible_ = true;
	if (m_popout_.is_open())
		m_popout_.raise();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Header row
//    docked:      title on the left,  [minimize] [Pop out] [X] on the right
//    popped out:  title on the left,  [Dock] on the right  (the OS title bar handles move / close)
// ─────────────────────────────────────────────────────────────────────────────
void ControlPanel::draw_header()
{
	const ImGuiStyle& style = ImGui::GetStyle();
	const float btn = ImGui::GetFrameHeight();

	const char* mode_label = m_popped_out_ ? "Dock" : "Pop out";
	const float mode_w = ImGui::CalcTextSize(mode_label).x + style.FramePadding.x * 2.f;

	ImGui::PushStyleColor(ImGuiCol_Text, { 0.72f, 0.84f, 1.00f, 1.f });
	ImGui::TextUnformatted(control_panel_window_name);
	ImGui::PopStyleColor();

	float total = mode_w;
	if (!m_popped_out_) total += btn + style.ItemSpacing.x + style.ItemSpacing.x + btn;   // [minimize] [mode] [X]

	ImGui::SameLine(ImGui::GetWindowWidth() - style.WindowPadding.x - total);

	if (!m_popped_out_)
	{
		if (ImGui::ArrowButton("##minimize", m_collapsed_ ? ImGuiDir_Down : ImGuiDir_Up))
			m_collapsed_ = !m_collapsed_;
		ImGui::SetItemTooltip("%s", m_collapsed_ ? "Expand" : "Minimize");
		ImGui::SameLine();
	}

	const bool can_toggle = m_popped_out_ || m_main_ != nullptr;
	ImGui::BeginDisabled(!can_toggle);
	if (ImGui::Button(mode_label))
		m_popped_out_ = !m_popped_out_;
	ImGui::EndDisabled();
	ImGui::SetItemTooltip("%s", m_popped_out_ ? "Snap the panel back into the main window"
		: "Move the panel into its own window");

	if (!m_popped_out_)
	{
		ImGui::SameLine();
		if (ImGui::Button("X", { btn, btn }))
			m_visible_ = false;
		ImGui::SetItemTooltip("Hide (F1 to restore)");
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tabs (identical in both modes)
// ─────────────────────────────────────────────────────────────────────────────
void ControlPanel::draw_tabs(const SimSnapshot& snap, ImGuiContext& ctx)
{
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
}

// ─────────────────────────────────────────────────────────────────────────────
//  Docked — the original in-window panel
// ─────────────────────────────────────────────────────────────────────────────
void ControlPanel::draw_docked(const SimSnapshot& snap, ImGuiContext& ctx)
{
	const ImVec2 disp = ImGui::GetIO().DisplaySize;
	const float PANEL_W = (4.f / 5.f) * disp.x;
	const float PANEL_H = (7.f / 20.f) * disp.y;

	ImGui::SetNextWindowPos({ 10.f, 10.f }, ImGuiCond_Always);
	ImGui::SetNextWindowSize({ PANEL_W, m_collapsed_ ? 0.f : PANEL_H }, ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.94f);

	constexpr ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoResize |          // user can't drag the window edges to resize it
		//ImGuiWindowFlags_NoMove |            // user can't drag the window around
		ImGuiWindowFlags_NoScrollbar |       // window itself never shows a scrollbar (child panels still can)
		//ImGuiWindowFlags_NoScrollWithMouse |  // (disabled) would stop the mouse wheel scrolling the window
		ImGuiWindowFlags_NoTitleBar |        // hides the title bar (you draw your own title text instead)
		ImGuiWindowFlags_NoSavedSettings;    // don't save/restore position and size in imgui.ini

	ImGui::Begin("##ARIACtrl", nullptr, flags);

	const bool was_popped_out = m_popped_out_;
	draw_header();

	// If "Pop out" was just clicked, skip the body: the panel moves to its own window next frame.
	if (m_popped_out_ == was_popped_out && !m_collapsed_)
	{
		draw_tabs(snap, ctx);
	}

	ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Popped out — the panel lives in a second OS window with its own ImGui context
// ─────────────────────────────────────────────────────────────────────────────
void ControlPanel::draw_popped_out(const SimSnapshot& snap, ImGuiContext& ctx)
{
	if (!m_popout_.is_open())
	{
		const ImVec2 disp = ImGui::GetIO().DisplaySize;
		const auto w = static_cast<unsigned>(std::max(k_popout_min_w, (4.f / 5.f) * disp.x));
		const auto h = static_cast<unsigned>(std::max(k_popout_min_h, (7.f / 20.f) * disp.y));

		if (!m_main_ || !m_popout_.open(*m_main_, w, h, control_panel_window_name, m_popout_init_))
		{
			m_popped_out_ = false;           // couldn't open: stay docked
			return;
		}
	}
	m_popout_.set_visible(true);

	// Placeholder left in the main window so there's always a way back.
	{
		constexpr ImGuiWindowFlags chip_flags =
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings;

		ImGui::SetNextWindowPos({ 10.f, 10.f }, ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.80f);
		ImGui::Begin("##ARIAPopped", nullptr, chip_flags);
		ImGui::TextDisabled("Control panel is in its own window");
		ImGui::SameLine();
		if (ImGui::Button("Dock"))
			m_popped_out_ = false;
		ImGui::End();
	}

	const bool alive = m_popout_.frame([&](ImVec2 size)
		{
			constexpr ImGuiWindowFlags flags =
				ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoBringToFrontOnFocus;

			ImGui::SetNextWindowPos({ 0.f, 0.f }, ImGuiCond_Always);
			ImGui::SetNextWindowSize(size, ImGuiCond_Always);

			if (ImGui::Begin("##ARIACtrlPop", nullptr, flags))
			{
				draw_header();
				draw_tabs(snap, ctx);
			}
			ImGui::End();
		});

	if (!alive)
		m_popped_out_ = false;               // OS window closed by the user == dock
}

// ─────────────────────────────────────────────────────────────────────────────
//  Draw
// ─────────────────────────────────────────────────────────────────────────────
void ControlPanel::draw(const SimSnapshot& snap, ImGuiContext& ctx, float dt)
{
	if (ImGui::IsKeyPressed(ImGuiKey_F1, false))
		m_visible_ = !m_visible_;

	// Docking (button, chip, or closing the OS window) tears the pop-out down.
	if (!m_popped_out_ && m_popout_.is_open())
		m_popout_.close();

	if (!m_visible_)
	{
		m_popout_.set_visible(false);

		constexpr ImGuiWindowFlags mini_flags =
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings;

		ImGui::SetNextWindowPos({ 10.f, 10.f }, ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.80f);
		ImGui::Begin("##ARIAShow", nullptr, mini_flags);
		if (ImGui::Button("Show panel (F1)"))
			m_visible_ = true;
		ImGui::End();
		return;
	}

	if (m_popped_out_)
		draw_popped_out(snap, ctx);
	else
		draw_docked(snap, ctx);
}