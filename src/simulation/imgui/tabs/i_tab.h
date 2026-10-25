#pragma once
#include "../../context/sim_snapshot.h"
#include "../../context/sim_command.h"
#include <optional>

// Horizontal progress bar with a colour override and an overlay string.
static void colored_bar(const float fraction, const ImVec4& color,
    const char* overlay, const ImVec2 size = { -1.f, 10.f })
{
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
    ImGui::ProgressBar(fraction, size, overlay);
    ImGui::PopStyleColor();
}


static void labeled_bar(const char* prefix, const float fraction,
    const ImVec4& color, const char* overlay, float k_summary_bar_height)
{
    ImGui::TextDisabled("%s", prefix);
    ImGui::SameLine();
    colored_bar(fraction, color, overlay, { -1.f, k_summary_bar_height });
}

// One cycle of sin(f*t + phase) is 2*pi/f frames
static int safe_time_period(const float frequency, int k_max_wave_buf)
{
    constexpr float two_pi = 6.28318530718f;
    if (std::abs(frequency) < 1e-6f) return 120;
    return std::clamp(static_cast<int>(std::lround(two_pi / std::abs(frequency))), 1, k_max_wave_buf);
}

// Analytical min/max of A*sin(...)+D clamped to [lo, hi].
// sin ranges over [-1, 1] so the wave spans [D-|A|, D+|A|].
static void wave_range(const float A, const float D, const float lo, const float hi,
    float& out_min, float& out_max)
{
    out_min = std::clamp(D - std::abs(A), lo, hi);
    out_max = std::clamp(D + std::abs(A), lo, hi);
}

// Green-to-red gradient: green at f=1, red at f=0.
static ImVec4 fraction_color(const float f)
{
    return f > 0.5f ? ImVec4{ 2.f * (1.f - f), 1.f, 0.2f, 1.f }
    : ImVec4{ 1.f, 2.f * f,     0.2f, 1.f };
}

static void colored_progress(const float fraction, const ImVec4 color,
    const char* label, const ImVec2 size = { -1.f, 10.f })
{
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
    ImGui::ProgressBar(fraction, size, label);
    ImGui::PopStyleColor();
}

struct ITab
{
    virtual ~ITab() = default;
    virtual const char* label() const = 0;
    // snap is READ-ONLY display data
    // toggles is a mutable COPY you can write into freely
    virtual void draw(const SimSnapshot& snap, ImGuiContext& ctx) = 0;

    // ── Toggle checkboxes, one overload per toggle struct ──────────────────
    // Overload resolution picks the right one from the type of `field`
    // (e.g. &WorldToggles::paused vs &FoodToggles::food_mitosis), so call
    // sites don't need to know or care which one they're hitting.

    void toggle(const SimSnapshot& snap, ImGuiContext& ctx, const char* label, bool WorldToggles::* field, const char* shortcut = nullptr)
    {
        if (auto updated = checkbox_toggle(snap.world_toggles, label, field, shortcut))
            ctx.push({ .section = CommandSection::WorldEvent, .type = CommandType::SetWorldToggles, .world_toggles = *updated });
    }

    void toggle(const SimSnapshot& snap, ImGuiContext& ctx, const char* label, bool CellManagerToggles::* field, const char* shortcut = nullptr)
    {
        if (auto updated = checkbox_toggle(snap.cell_toggles, label, field, shortcut))
            ctx.push({ .section = CommandSection::CellManagerEvent, .type = CommandType::SetCellToggles, .cell_toggles = *updated });
    }

    void toggle(const SimSnapshot& snap, ImGuiContext& ctx, const char* label, bool FoodToggles::* field, const char* shortcut = nullptr)
    {
        if (auto updated = checkbox_toggle(snap.food_toggles, label, field, shortcut))
            ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetFoodToggles, .food_toggles = *updated });
    }

    void toggle(const SimSnapshot& snap, ImGuiContext& ctx, const char* label, bool SimulationToggles::* field, const char* shortcut = nullptr)
    {
        if (auto updated = checkbox_toggle(snap.sim_toggles, label, field, shortcut))
            ctx.push({ .section = CommandSection::SimulationEvent, .type = CommandType::SetSimToggles, .sim_toggles = *updated });
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

private:
    // Shared checkbox+shortcut logic for all four toggle() overloads.
    // Returns the updated struct only if the checkbox was actually clicked,
    // so callers can tell "no change" apart from "changed to false".
    template <typename ToggleT>
    std::optional<ToggleT> checkbox_toggle(const ToggleT& current, const char* label, bool ToggleT::* field, const char* shortcut)
    {
        bool val = current.*field;
        const bool changed = ImGui::Checkbox(label, &val);

        if (shortcut)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("[%s]", shortcut);
        }

        if (!changed)
            return std::nullopt;

        ToggleT updated = current;
        updated.*field = val;
        return updated;
    }
};