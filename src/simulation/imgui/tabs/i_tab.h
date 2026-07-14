#pragma once
#include "../../context/sim_snapshot.h"
#include "../../context/sim_command.h"


struct ITab
{
    virtual ~ITab() = default;
    virtual const char* label() const = 0;
    // snap is READ-ONLY display data
    // toggles is a mutable COPY you can write into freely
    virtual void draw(const SimSnapshot& snap, ImGuiContext& ctx) = 0;

    void toggle(const SimSnapshot& snap, ImGuiContext& ctx, const char* label, bool WorldToggles::* field, const char* shortcut = nullptr)
    {
        bool val = snap.toggles.*field;
        if (ImGui::Checkbox(label, &val))
        {
            WorldToggles new_toggles = snap.toggles;
            new_toggles.*field = val;
            ctx.push({ .section = CommandSection::WorldEvent, .type = CommandType::SetWorldToggles, .toggles = new_toggles });
        }
        if (shortcut)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("[%s]", shortcut);
        }
    }


    // Draws a single button that is visually "pressed" when current_value == option_value,
// and on click pushes a SimCommand carrying option_value in the given SimCommand field.
// ValueT must match the type of `field` (int, float, bool, or a scoped enum stored as int).
    template <typename ValueT>
    bool mode_button(ImGuiContext& ctx, CommandSection section, CommandType type,
        ValueT SimCommand::* field, const char* label,
        ValueT current_value, ValueT option_value,
        const ImVec4& active_color, const ImVec2& size = { 0.f, 0.f })
    {
        const bool is_active = (current_value == option_value);

        if (is_active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, active_color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active_color);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, active_color);
        }

        const bool clicked = ImGui::Button(label, size);

        if (is_active)
            ImGui::PopStyleColor(3);

        if (clicked)
        {
            SimCommand cmd{ .section = section, .type = type };
            cmd.*field = option_value;
            ctx.push(cmd);
        }

        return clicked;
    }

    // One entry in a mutually-exclusive mode-button row.
    template <typename ValueT>
    struct ModeOption
    {
        const char* label;
        ValueT      value;
    };

    template <typename ValueT, size_t N>
    void mode_button_row(ImGuiContext& ctx, CommandSection section, CommandType type,
        ValueT SimCommand::* field, const ModeOption<ValueT>(&options)[N],
        ValueT current_value, const ImVec4& active_color, float height = 0.f)
    {
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float total_w = ImGui::GetContentRegionAvail().x;
        const float btn_w = (total_w - spacing * static_cast<float>(N - 1)) / static_cast<float>(N);

        for (size_t i = 0; i < N; ++i)
        {
            if (i > 0) ImGui::SameLine();
            mode_button(ctx, section, type, field, options[i].label,
                current_value, options[i].value, active_color, { btn_w, height });
        }
    }

    void slider_float_cmd(ImGuiContext& ctx, const char* label, float current, float min, float max,
        const char* fmt, CommandSection section, CommandType type)
        {
            float val = current;
            if (ImGui::SliderFloat(label, &val, min, max, fmt))
            {
                SimCommand cmd;
                cmd.section = section;
                cmd.type = type;
                cmd.float_val = val;
                ctx.push(cmd);
            }
        };

};