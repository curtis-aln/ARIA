#include "tools_tab.h"
#include "../helpers/confirm_button.h"
#include <imgui.h>
#include <algorithm>

void ToolsTab::draw(const SimSnapshot& snap, ImGuiContext& ctx)
{
    draw_blackhole();
    ImGui::Spacing();
    draw_spawn(snap);
    ImGui::Spacing();
    draw_clear(snap);
}

void ToolsTab::draw_blackhole()
{
    ImGui::SeparatorText("Blackhole Tool (stub)");
    if (m_blackhole_) ImGui::PushStyleColor(ImGuiCol_Button, { 0.5f, 0.1f, 0.7f, 1.f });
    if (ImGui::Button(m_blackhole_ ? "Deactivate Blackhole##bh" : "Activate Blackhole##bh", { -1.f, 0.f }))
        m_blackhole_ = !m_blackhole_;
    if (m_blackhole_) ImGui::PopStyleColor();
    if (m_blackhole_) ImGui::TextColored({ 0.9f, 0.5f, 1.f, 1.f }, "Click in world to place");
    ImGui::SetNextItemWidth(-1.f); ImGui::SliderFloat("Strength##bh", &m_bh_strength_, 10.f, 5000.f, "%.0f");
    ImGui::SetNextItemWidth(-1.f); ImGui::SliderFloat("Radius##bh", &m_bh_radius_, 100.f, 5000.f, "%.0f");
    ImGui::TextDisabled("TODO: World::apply_gravity_well(pos, strength, radius)");
}

void ToolsTab::draw_spawn(const SimSnapshot& snapshot)
{
    ImGui::SeparatorText("Spawn");
    ImGui::SetNextItemWidth(80.f); ImGui::InputInt("Count##sp", &m_spawn_n_);
    m_spawn_n_ = std::max(1, m_spawn_n_);

    ImGui::Checkbox("Mutate on spawn##sp", &m_spawn_mut_);
    if (m_spawn_mut_)
    {
        ImGui::SetNextItemWidth(-1.f); ImGui::SliderFloat("Mut rate##sp", &m_mut_rate_, 0.f, 1.f);
        ImGui::SetNextItemWidth(-1.f); ImGui::SliderFloat("Mut range##sp", &m_mut_range_, 0.f, 1.f);
    }

    if (ImGui::Button("Spawn N random organisms##sp", { -1.f, 0.f }))
    { /* TODO: ctx.world.spawn_random(m_spawn_n_) */
    }
    if (ImGui::Button("Spawn from saved file (stub)##sp", { -1.f, 0.f }))
    { /* TODO */
    }
    if (ImGui::Button("Fill world with saved organism (stub)##sp", { -1.f, 0.f }))
    { /* TODO */
    }
    if (ImGui::Button("Spawn food cluster at cursor (stub)##sp", { -1.f, 0.f }))
    { /* TODO: ctx.world.get_food_manager()->spawn_cluster(...) */
    }
}

void ToolsTab::draw_clear(const SimSnapshot& snapshot)
{
    ImGui::SeparatorText("Clear");
    if (ConfirmButton::draw("Clear all food##tools", { -1.f, 0.f }))
    { /* TODO: ctx.world.get_food_manager()->clear_all() */
    }
    if (ConfirmButton::draw("Clear all protozoa##tools", { -1.f, 0.f }))
    { /* TODO: ctx.world.clear_all_protozoa() */
    }
}