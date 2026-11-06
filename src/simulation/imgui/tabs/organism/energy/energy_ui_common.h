#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Shared UI helpers for the Organism "Energy" tab.
//  Header-only (inline) so every organism_tab_energy*.cpp can use them without
//  duplicating anonymous-namespace copies. Helpers used by a single file stay
//  local to that file.
// ─────────────────────────────────────────────────────────────────────────────
#include "../../graphs/plot_utils.h"
#include "../organism_tab.h"
#include <algorithm>

namespace en_ui
{
	inline ImU32 u32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }
	inline ImU32 u32(const ImVec4& c, float a) { return ImGui::ColorConvertFloat4ToU32({ c.x, c.y, c.z, a }); }

	inline float safe_frac(float num, float den) { return den > 0.f ? std::clamp(num / den, 0.f, 1.f) : 0.f; }

	// Rounded, slightly lighter child panel. Style is applied at BeginChild time,
	// so it is popped straight away. Caller must always call EndChild().
	inline bool begin_card(const char* id, float w)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8.f, 6.f });
		ImGui::PushStyleColor(ImGuiCol_ChildBg, k_en_col_card_bg);
		ImGui::PushStyleColor(ImGuiCol_Border, k_en_col_card_border);
		const bool open = ImGui::BeginChild(id, { w, -1.f }, true, ImGuiWindowFlags_NoScrollbar);
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(2);
		return open;
	}
}