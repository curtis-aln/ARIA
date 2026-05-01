#include "tagged_tab.h"
#include <imgui.h>
#include <vector>
#include <string>
#include <algorithm>

void TaggedTab::draw(const SimSnapshot& snap, ImGuiContext& ctx)
{
    draw_tag_input(snap);
    ImGui::Spacing();
    draw_list(ctx, snap);
}

void TaggedTab::draw_tag_input(const SimSnapshot& snapshot)
{
    ImGui::SeparatorText("Tag by ID");
    static int input_id = 0;
    ImGui::SetNextItemWidth(100.f);
    ImGui::InputInt("##tag_in", &input_id);
    ImGui::SameLine();
    if (ImGui::Button("Toggle Tag##tagged")) toggle_tag(input_id);

	const ProtozoaTracker& sel = snapshot.protozoa_tracker;
    if (snapshot.selected_a_protozoa)
    {
        ImGui::SameLine(0, 16);
        if (ImGui::Button(is_tagged(sel.id) ? "Untag selected" : "Tag selected"))
            toggle_tag(sel.id);
    }
}

void TaggedTab::draw_list(ImGuiContext& ctx, const SimSnapshot& snapshot)
{
    if (m_tagged_ids_.empty()) { ImGui::TextDisabled("No organisms tagged."); return; }
    ImGui::SeparatorText("Tagged organisms");

    std::vector<int> to_remove;

    for (int id : m_tagged_ids_)
    {
        ImGui::PushID(id);

        if (snapshot.selected_a_protozoa)
        {
            auto p = snapshot.protozoa_tracker;
            const float ef = std::clamp(p.total_energy / 300.f, 0.f, 1.f);
            ImGui::Text("ID %-4d", id); ImGui::SameLine(0, 8);
            ImGui::Text("Age %-6u", p.frames_alive); ImGui::SameLine(0, 8);
            ImGui::ProgressBar(ef, { 60.f, 8.f }, ""); ImGui::SameLine(0, 8);
            ImGui::Text("Off %-3d", p.offspring_count); ImGui::SameLine(0, 8);
            ImGui::Text("Gen %.0f", p.average_generation);
            ImGui::SameLine(0, 8);
            if (ImGui::SmallButton("Go##tagged"))
            {
                SimCommand cmd{ CommandType::NavToProtozoa };
				cmd.int_val = id;
                ctx.push(cmd);
            }
        }
        else
        {
            ImGui::TextDisabled("ID %-4d  [dead/missing]", id);
            to_remove.push_back(id);
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("X##tagged")) to_remove.push_back(id);

        ImGui::PopID();
    }

    for (int id : to_remove) m_tagged_ids_.erase(id);
}

void TaggedTab::toggle_tag(const int id)
{
    if (m_tagged_ids_.count(id)) m_tagged_ids_.erase(id);
    else                          m_tagged_ids_.insert(id);
}

bool TaggedTab::is_tagged(const int id) const { return m_tagged_ids_.count(id) > 0; }

void TaggedTab::notify_death(const int id, const std::string& cause)
{
    if (!is_tagged(id)) return;
    m_toasts_.push("Tagged #" + std::to_string(id) + " died: " + cause,
        { 1.f, 0.4f, 0.4f, 1.f }, 6.f);
}

void TaggedTab::draw_toasts(const float dt) { m_toasts_.update(dt); m_toasts_.draw(); }