#pragma once
#include <imgui.h>

// Two-column aligned stat display.
// Label left-aligned, value at a fixed offset.
namespace StatRow
{
    static constexpr float k_col = 155.f;

    template<typename... Args>
    inline void draw(const char* label, const char* fmt, Args... args)
    {
        ImGui::TextDisabled("%s", label);
        ImGui::SameLine(k_col);
        ImGui::Text(fmt, args...);
    }

    template<typename... Args>
    inline void draw_warn(const char* label, bool warn, const char* fmt, Args... args)
    {
        ImGui::TextDisabled("%s", label);
        ImGui::SameLine(k_col);
        if (warn) ImGui::PushStyleColor(ImGuiCol_Text, { 1.f, 0.3f, 0.3f, 1.f });
        ImGui::Text(fmt, args...);
        if (warn) ImGui::PopStyleColor();
    }
}