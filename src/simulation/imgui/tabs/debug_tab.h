#pragma once

#include "../../../world/world_settings.h"
#include "i_tab.h"
#include <simulation/context/sim_command.h>
#include <simulation/context/sim_snapshot.h>

class DebugTab : public ITab
{
public:
	const char* label() const override { return "Debug"; }
	void        draw(const SimSnapshot& snap, ImGuiContext& ctx)   override;


	int max_bounds_radius = WorldSettings::bounds_radius;
};