#pragma once
#include <imgui.h>
#include <unordered_map>
#include <string>

// Stateful inline confirmation widget.
// Usage:  if (ConfirmButton::draw("Delete##id")) { /* confirmed */ }
namespace ConfirmButton
{
    inline std::unordered_map<std::string, bool> s_pending{};

    inline bool draw(const char* label, ImVec2 size = { 0.f, 0.f })
    {
        const std::string key(label);
        if (s_pending.count(key) && s_pending[key])
        {
            ImGui::PushStyleColor(ImGuiCol_Button, { 0.7f, 0.1f, 0.1f, 1.f });
            const bool yes = ImGui::Button(("Yes##" + key).c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine();
            if (ImGui::Button(("No##" + key).c_str()) || yes)
                s_pending.erase(key);
            return yes;
        }
        if (ImGui::Button(label, size))
            s_pending[key] = true;
        return false;
    }
}