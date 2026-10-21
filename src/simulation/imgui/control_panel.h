#pragma once
#include "../context/sim_snapshot.h"
#include "tabs/i_tab.h"
#include <memory>
#include <simulation/context/sim_command.h>
#include <vector>


class ControlPanel
{
public:
	ControlPanel();

	void draw(const SimSnapshot& snap, ImGuiContext& ctx, float dt);

	// Request that a specific tab become active on the next draw() call.
	void select_tab(const char* label);

private:
	std::vector<std::unique_ptr<ITab>> m_tabs_;
	const char* m_pending_tab_label_ = nullptr;

	bool m_collapsed_ = false;
	bool m_visible_ = true;
};