#pragma once
#include <imgui.h>
#include <vector>
#include <string>
#include <algorithm>

struct Toast
{
    std::string message;
    ImVec4      color = { 1.f, 1.f, 1.f, 1.f };
    float       duration = 4.f;
    float       elapsed = 0.f;
};

struct ToastSystem
{
    static constexpr int   k_max = 6;
    static constexpr float k_fade = 0.75f; // fade after 75% of duration

    std::vector<Toast> toasts;

    void push(std::string msg, ImVec4 color = { 1.f, 0.85f, 0.3f, 1.f }, float dur = 4.f)
    {
        if ((int)toasts.size() >= k_max) toasts.erase(toasts.begin());
        toasts.push_back({ std::move(msg), color, dur, 0.f });
    }

    void update(float dt)
    {
        for (auto& t : toasts) t.elapsed += dt;
        toasts.erase(std::remove_if(toasts.begin(), toasts.end(),
            [](const Toast& t) { return t.elapsed >= t.duration; }), toasts.end());
    }

    void draw() const
    {
        if (toasts.empty()) return;
        const ImGuiIO& io = ImGui::GetIO();
        ImVec2 pos = { io.DisplaySize.x - 320.f, 10.f };

        for (const Toast& t : toasts)
        {
            const float progress = t.elapsed / t.duration;
            const float alpha = progress < k_fade
                ? 1.f
                : 1.f - (progress - k_fade) / (1.f - k_fade);

            ImGui::SetNextWindowPos(pos);
            ImGui::SetNextWindowBgAlpha(alpha * 0.88f);
            ImGui::SetNextWindowSize({ 300.f, 0.f });

            const std::string wid = "##toast" + std::to_string(reinterpret_cast<size_t>(&t));
            ImGui::Begin(wid.c_str(), nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoFocusOnAppearing);

            ImVec4 col = t.color; col.w = alpha;
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextWrapped("%s", t.message.c_str());
            ImGui::PopStyleColor();

            pos.y += ImGui::GetWindowHeight() + 4.f;
            ImGui::End();
        }
    }
};