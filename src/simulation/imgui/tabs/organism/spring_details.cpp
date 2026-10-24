#include "organism_tab.h"


// ─────────────────────────────────────────────────────────────────────────────
//  Spring detail (stats + sin wave)
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_spring_detail(ImGuiContext& ctx, const OrganismTracker& p, const Spring& s)
{
    const int period = safe_time_period(s.genome.frequency, k_max_wave_buf);
    const int display_size = std::min(m_wave_cycles_ * period, k_max_wave_buf);
    const int head = static_cast<int>(p.frames_alive) % display_size;
    float ext_min, ext_max;
    wave_range(s.genome.amplitude, s.genome.vertical_shift, 0.f, 1.f, ext_min, ext_max);


    // ── Stats ─────────────────────────────────────────────────────────────
    ImGui::BeginChild("SL_stat", spring_cell_box_size, true);

    ImGui::TextDisabled("Spring %d->%d", s.cell_A_id, s.cell_B_id);

    const float length_diff = s.rest_length - s.current_length;
    ImGui::Text("age: %d", s.internal_clock_);
    ImGui::Text("Rest L:  %.1f, Real L: %.1f", s.rest_length, s.current_length);
    ImGui::Text("Length Diff:  %.2f", length_diff);
    ImGui::Text("Period        %d frames", period);
    ImGui::Text("Extension min %.0f  max %.0f", ext_min, ext_max);
    ImGui::Text("Mutation R    %.4f  Rng %.4f", s.genome.guassian_const, s.genome.mutation_range);

    ImGui::Spacing();
    ImGui::TextDisabled("Forces");
    ImGui::Text("Spring Force %.2f", s.spring_force);
    ImGui::Text("Damping Force %.2f", s.damping_force);
    ImGui::Text("Total Force %.2f", s.spring_force + s.damping_force);

    const float total_force = s.spring_force + s.damping_force;
    const float ext_range = ext_max - ext_min;

    // Drawing the Force and Extension Progress Bars
    static float max_spring_const = SpringGeneticConstraints::spring_const.max;
    float force_scale = max_spring_const > 0.f
        ? 1.f / max_spring_const : 1.f;
    const float ext_scale = ext_range > 0.f ? 1.f / ext_range : 1.f;

    auto bar_with_label = [](float frac, const char* text)
        {
            const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
            // fixed width so the bar doesn't jitter as the digits change
            const float label_w = ImGui::CalcTextSize("Force  -000.00").x;
            ImGui::ProgressBar(std::clamp(frac, 0.f, 1.f), { -(label_w + spacing), 8.f }, "");
            ImGui::SameLine(0.f, spacing);
            ImGui::TextUnformatted(text);
        };

    char buf[32];

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, { 0.4f, 0.8f, 1.f, 1.f });

    snprintf(buf, sizeof buf, "Force  %.2f", total_force);
    bar_with_label(total_force * force_scale, buf);

    snprintf(buf, sizeof buf, "Ext    %.2f", s.current_length - s.rest_length);
    bar_with_label((s.current_length - ext_min) * ext_scale, buf);

    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::TextDisabled("Physical");
    ImGui::SetNextItemWidth(-1.f);

    slider_float_cmd(ctx, "##sk", s.genome.spring_const,
        0.f, SpringGeneticConstraints::spring_const.max,
        "Spring constant = %.3f", CommandSection::CellManagerEvent, CommandType::SetSpringConst);

    ImGui::SetNextItemWidth(-1.f);
    slider_float_cmd(ctx, "##sd", s.genome.damping,
        0.f, SpringGeneticConstraints::damping.max,
        "Damping         = %.3f", CommandSection::CellManagerEvent, CommandType::SetDampingConst);

    ImGui::EndChild();
    ImGui::SameLine();

    // ── Sin wave ───────────────────────────────────────────────────────────
    static std::vector<float> ext_buf;
    draw_wave_panel(ctx, 0.f, "SL_wave",
        "Extension  amplitude * sin(frequency * t + phase) + shift  [0, 1]",
        static_cast<int>(p.frames_alive), s.id_, "ratio", ext_buf,
        { s.genome.amplitude,      0.f,                                          SpringGeneticConstraints::amplitude.max,      "Amplitude = %.3f", CommandType::SetSpringAmplitude },
        { s.genome.frequency,      -SpringGeneticConstraints::frequency.min,     SpringGeneticConstraints::frequency.max,      "Frequency = %.5f", CommandType::SetSpringFrequency },
        { s.genome.offset,         -SpringGeneticConstraints::offset.min,        SpringGeneticConstraints::offset.max,         "Phase     = %.3f", CommandType::SetSpringOffset },
        { s.genome.vertical_shift, -SpringGeneticConstraints::vertical_shift.min, SpringGeneticConstraints::vertical_shift.max, "Shift     = %.3f", CommandType::SetSpringVerticalShift });

}
