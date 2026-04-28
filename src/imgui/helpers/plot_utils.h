#pragma once
#include <imgui.h>
#include <algorithm>
#include <string>

namespace PlotUtils
{
    inline void fill_sinwave(float* out, const int time_period,
        float A, float B, float C, float D,
        float lo, float hi)
    {
        for (int i = 0; i < time_period; ++i)
            out[i] = std::clamp(A * sinf(B * static_cast<float>(i) + C) + D, lo, hi);
    }

    inline void sinwave_graph(const char* id, const float* buf, int time_period, int playhead,
        float y_min, float y_max, ImVec2 size = { -1.f, 52.f })
    {
        ImGui::PlotLines(id, buf, time_period, 0, nullptr, y_min, y_max, size);
        const ImVec2 rmin = ImGui::GetItemRectMin();
        const ImVec2 rmax = ImGui::GetItemRectMax();
        const float  xf = rmin.x + (playhead / (float)(time_period - 1)) * (rmax.x - rmin.x);
        const float  t = 1.f - std::clamp((buf[playhead] - y_min) / (y_max - y_min), 0.f, 1.f);
        const float  yf = rmin.y + t * (rmax.y - rmin.y);
        auto* dl = ImGui::GetWindowDrawList();
        dl->AddLine({ xf, rmin.y }, { xf, rmax.y }, IM_COL32(255, 65, 65, 210), 1.5f);
        dl->AddCircleFilled({ xf, yf }, 3.5f, IM_COL32(255, 80, 80, 255));
    }

    inline std::string format_time(float seconds)
    {
        const int s = static_cast<int>(seconds);
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", s / 3600, (s % 3600) / 60, s % 60);
        return buf;
    }
}