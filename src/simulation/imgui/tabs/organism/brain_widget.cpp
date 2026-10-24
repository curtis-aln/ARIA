#include "organism_tab.h"


// ─────────────────────────────────────────────────────────────────────────────
//  Brain — draws Input layer <-> Output layer with weighted connections
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::Brain()
{
    // ── Persistent state (function-local statics, survive across frames) ───
    static int   input_size = 4;
    static int   output_size = 3;
    static std::vector<float> input_activations;
    static std::vector<float> output_activations;
    static std::vector<float> weights;       // flattened [input_size * output_size]
    static bool  initialized = false;
    static bool  animate = false;
    static float anim_t = 0.f;

    auto rand01 = []() { return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX); };

    // Regenerates weights + a fake forward pass whenever size/refresh changes.
    auto randomize = [&]()
        {
            input_activations.resize(input_size);
            output_activations.assign(output_size, 0.f);
            weights.resize(static_cast<size_t>(input_size) * output_size);

            for (float& a : input_activations) a = rand01();
            for (float& w : weights)            w = rand01() * 2.f - 1.f; // range [-1, 1]

            for (int o = 0; o < output_size; ++o)
            {
                float sum = 0.f;
                for (int i = 0; i < input_size; ++i)
                    sum += input_activations[i] * weights[i * output_size + o];
                output_activations[o] = 1.f / (1.f + std::exp(-sum)); // sigmoid squash
            }
        };

    if (!initialized)
    {
        std::srand(static_cast<unsigned>(std::time(nullptr)));
        randomize();
        initialized = true;
    }

    // ── Design constants ────────────────────────────────────────────────────
    constexpr float k_neuron_radius = 16.f;
    constexpr float k_layer_gap = 260.f;
    constexpr float k_neuron_gap = 46.f;
    constexpr float k_canvas_height = 380.f;

    constexpr ImVec4 k_input_color = { 0.30f, 0.65f, 1.00f, 1.f };
    constexpr ImVec4 k_output_color = { 1.00f, 0.55f, 0.25f, 1.f };
    constexpr ImVec4 k_pos_weight_col = { 0.30f, 0.85f, 0.35f, 1.f };
    constexpr ImVec4 k_neg_weight_col = { 0.90f, 0.25f, 0.25f, 1.f };

    // Activation drives alpha: dim = low activation, bright = high activation.
    auto activation_fill = [](const ImVec4& base, float a)
        {
            a = std::clamp(a, 0.f, 1.f);
            return ImVec4{ base.x, base.y, base.z, 0.15f + 0.85f * a };
        };

    // ── Not-implemented disclaimer ───────────────────────────────────────────
    {
        const float h = ImGui::GetTextLineHeightWithSpacing() * 3.f
            + ImGui::GetStyle().WindowPadding.y * 2.f;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0.35f, 0.22f, 0.02f, 0.95f });
        ImGui::PushStyleColor(ImGuiCol_Border, { 1.00f, 0.75f, 0.20f, 1.00f });
        ImGui::BeginChild("Brain_disclaimer", { -1.f, h }, true, ImGuiWindowFlags_NoScrollbar);

        ImGui::TextColored({ 1.f, 0.85f, 0.3f, 1.f }, "NOT IMPLEMENTED YET");
        ImGui::TextWrapped("This tab is a UI prototype. The network below is randomly generated "
            "and is NOT connected to any organism's brain.");

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
    }

    // ═════════════════════════════════════════════════════════════════════
    //  Controls / legend / stats panel
    // ═════════════════════════════════════════════════════════════════════
    ImGui::BeginChild("Brain_controls", { 220.f, k_canvas_height }, true);
    ImGui::TextDisabled("Network Shape");
    ImGui::Separator();

    bool shape_changed = false;
    ImGui::SetNextItemWidth(-1.f);
    shape_changed |= ImGui::SliderInt("##in_size", &input_size, 1, 12, "Inputs  = %d");
    ImGui::SetNextItemWidth(-1.f);
    shape_changed |= ImGui::SliderInt("##out_size", &output_size, 1, 12, "Outputs = %d");
    if (shape_changed) randomize();

    ImGui::Spacing();
    if (ImGui::Button("Randomize", { -1.f, 0.f })) randomize();
    ImGui::Checkbox("Animate", &animate);

    ImGui::Spacing();
    ImGui::TextDisabled("Legend");
    ImGui::Separator();
    ImGui::ColorButton("##leg_in", k_input_color, 0, { 14.f, 14.f });
    ImGui::SameLine(); ImGui::Text("Input neuron");
    ImGui::ColorButton("##leg_out", k_output_color, 0, { 14.f, 14.f });
    ImGui::SameLine(); ImGui::Text("Output neuron");
    ImGui::ColorButton("##leg_pos", k_pos_weight_col, 0, { 14.f, 14.f });
    ImGui::SameLine(); ImGui::Text("Positive weight");
    ImGui::ColorButton("##leg_neg", k_neg_weight_col, 0, { 14.f, 14.f });
    ImGui::SameLine(); ImGui::Text("Negative weight");
    ImGui::TextDisabled("(brighter fill = higher activation)");
    ImGui::TextDisabled("(thicker line = larger |weight|)");

    ImGui::Spacing();
    ImGui::TextDisabled("Stats");
    ImGui::Separator();
    const float avg_in = input_activations.empty() ? 0.f :
        std::accumulate(input_activations.begin(), input_activations.end(), 0.f) / static_cast<float>(input_activations.size());
    const float avg_out = output_activations.empty() ? 0.f :
        std::accumulate(output_activations.begin(), output_activations.end(), 0.f) / static_cast<float>(output_activations.size());
    ImGui::Text("Avg input act.   %.3f", avg_in);
    ImGui::Text("Avg output act.  %.3f", avg_out);
    ImGui::Text("Weight count     %d", static_cast<int>(weights.size()));

    ImGui::EndChild();
    ImGui::SameLine();

    // ═════════════════════════════════════════════════════════════════════
    //  Canvas — neurons + connections
    // ═════════════════════════════════════════════════════════════════════
    ImGui::BeginChild("Brain_canvas", { -1.f, k_canvas_height }, true, ImGuiWindowFlags_NoScrollbar);

    if (animate)
    {
        anim_t += ImGui::GetIO().DeltaTime;
        if (anim_t > 1.5f) { randomize(); anim_t = 0.f; }
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();

    // Vertically centers a layer of `count` neurons around the canvas midline.
    auto layer_y = [&](int idx, int count) -> float
        {
            const float total_h = static_cast<float>(count - 1) * k_neuron_gap;
            return origin.y + (avail.y - total_h) * 0.5f + static_cast<float>(idx) * k_neuron_gap;
        };

    const float in_x = origin.x + 60.f;
    const float out_x = origin.x + std::max(avail.x - 60.f, in_x + k_layer_gap);

    std::vector<ImVec2> in_pos(input_size), out_pos(output_size);
    for (int i = 0; i < input_size; ++i) in_pos[i] = { in_x,  layer_y(i, input_size) };
    for (int o = 0; o < output_size; ++o) out_pos[o] = { out_x, layer_y(o, output_size) };

    // ── Connections (drawn first so neurons sit on top) ─────────────────────
    for (int i = 0; i < input_size; ++i)
    {
        for (int o = 0; o < output_size; ++o)
        {
            const float w = weights[i * output_size + o];
            const ImVec4 col = w >= 0.f ? k_pos_weight_col : k_neg_weight_col;
            const float mag = std::min(std::fabs(w), 1.f);
            const float thickness = 0.5f + mag * 2.5f;
            const float alpha = 0.20f + mag * 0.55f;

            draw->AddLine(in_pos[i], out_pos[o],
                ImGui::ColorConvertFloat4ToU32({ col.x, col.y, col.z, alpha }), thickness);
        }
    }

    // ── Input neurons ────────────────────────────────────────────────────
    for (int i = 0; i < input_size; ++i)
    {
        const ImVec4 fill = activation_fill(k_input_color, input_activations[i]);
        draw->AddCircleFilled(in_pos[i], k_neuron_radius, ImGui::ColorConvertFloat4ToU32(fill));
        draw->AddCircle(in_pos[i], k_neuron_radius, ImGui::ColorConvertFloat4ToU32(k_input_color), 0, 1.5f);

        char lbl[8]; snprintf(lbl, sizeof(lbl), "I%d", i);
        const ImVec2 ts = ImGui::CalcTextSize(lbl);
        draw->AddText({ in_pos[i].x - k_neuron_radius - ts.x - 6.f, in_pos[i].y - ts.y * 0.5f },
            IM_COL32_WHITE, lbl);

        ImGui::SetCursorScreenPos({ in_pos[i].x - k_neuron_radius, in_pos[i].y - k_neuron_radius });
        ImGui::PushID(i);
        ImGui::InvisibleButton("in_hit", { k_neuron_radius * 2.f, k_neuron_radius * 2.f });
        ImGui::PopID();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Input %d\nActivation: %.3f", i, input_activations[i]);
    }

    // ── Output neurons ───────────────────────────────────────────────────
    for (int o = 0; o < output_size; ++o)
    {
        const ImVec4 fill = activation_fill(k_output_color, output_activations[o]);
        draw->AddCircleFilled(out_pos[o], k_neuron_radius, ImGui::ColorConvertFloat4ToU32(fill));
        draw->AddCircle(out_pos[o], k_neuron_radius, ImGui::ColorConvertFloat4ToU32(k_output_color), 0, 1.5f);

        char lbl[8]; snprintf(lbl, sizeof(lbl), "O%d", o);
        const ImVec2 ts = ImGui::CalcTextSize(lbl);
        draw->AddText({ out_pos[o].x + k_neuron_radius + 6.f, out_pos[o].y - ts.y * 0.5f },
            IM_COL32_WHITE, lbl);

        ImGui::SetCursorScreenPos({ out_pos[o].x - k_neuron_radius, out_pos[o].y - k_neuron_radius });
        ImGui::PushID(o + input_size); // keep IDs unique vs input loop
        ImGui::InvisibleButton("out_hit", { k_neuron_radius * 2.f, k_neuron_radius * 2.f });
        ImGui::PopID();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Output %d\nActivation: %.3f", o, output_activations[o]);
    }

    ImGui::EndChild();
}