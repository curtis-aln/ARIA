#include "organism_tab.h"
#include <algorithm>
#include <cstdio>

void OrganismTab::draw_spring_detail(ImGuiContext& ctx, const OrganismTracker& p, const Spring& s, const int spring_rel)
{
    const int period = safe_time_period(s.genome.frequency, k_max_wave_buf);
    float ratio_min, ratio_max;
    wave_range(s.genome.amplitude, s.genome.vertical_shift, 0.f, 1.f, ratio_min, ratio_max);
    const float break_len = s.spring_break_length_factor * s.maximum_extension;

    ImGui::BeginChild("SL_stat", spring_cell_box_size, true);

    const int ra = relative_cell_index(s.cell_A_id), rb = relative_cell_index(s.cell_B_id);
    ImGui::TextDisabled("Spring %d", spring_rel);
    ImGui::Text("Cell %d -> Cell %d", ra, rb);

    ImGui::Spacing();
    ImGui::TextDisabled("Absolute IDs");
    ImGui::Separator();
    ImGui::Text("Spring  %u", static_cast<unsigned>(s.id_));
    ImGui::Text("Cell A  %u", static_cast<unsigned>(s.cell_A_id));
    ImGui::Text("Cell B  %u", static_cast<unsigned>(s.cell_B_id));

    ImGui::Spacing();
    ImGui::TextDisabled("State");
    ImGui::Separator();
    ImGui::Text("age: %u", s.internal_clock_);
    ImGui::Text("Rest L:  %.1f, Real L: %.1f", s.rest_length, s.current_length);
    ImGui::Text("Stretch:  %.2f", s.current_length - s.rest_length);
    ImGui::Text("Period        %d frames", period);
    ImGui::Text("Rest range    %.1f - %.1f", ratio_min * s.maximum_extension, ratio_max * s.maximum_extension);
    ImGui::Text("Mutation R    %.4f  Rng %.4f", s.genome.guassian_const, s.genome.mutation_range);

    ImGui::Spacing();
    ImGui::TextDisabled("Forces");
    ImGui::Text("Spring Force %.2f", s.spring_force);
    ImGui::Text("Damping Force %.2f", s.damping_force);
    ImGui::Text("Total Force %.2f", s.spring_force + s.damping_force);

    auto bar_with_label = [](float frac, const char* text)
        {
            const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
            const float label_w = ImGui::CalcTextSize("Length 0000 / 0000").x;
            ImGui::ProgressBar(std::clamp(frac, 0.f, 1.f), { -(label_w + spacing), 8.f }, "");
            ImGui::SameLine(0.f, spacing);
            ImGui::TextUnformatted(text);
        };

    char buf[40];
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, { 0.4f, 0.8f, 1.f, 1.f });
    snprintf(buf, sizeof buf, "Stress %.0f%%", s.stress * 100.f);
    bar_with_label(s.stress, buf);
    snprintf(buf, sizeof buf, "Length %.0f / %.0f", s.current_length, break_len);
    bar_with_label(s.current_length / break_len, buf);
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

    // The leading minus signs on .min were removed: they made offset/shift sliders degenerate.
    draw_wave_panel(ctx, "SL_wave",
        "Extension  amplitude * sin(frequency * t + phase) + shift  [0, 1]",
        static_cast<double>(s.internal_clock_), s.id_, "Extension",
        std::clamp(s.ratio, 0.f, 1.f), extension_wave_col,
        { s.genome.amplitude,      0.f,                                         SpringGeneticConstraints::amplitude.max,      "Amplitude = %.3f", CommandType::SetSpringAmplitude },
        { s.genome.frequency,      SpringGeneticConstraints::frequency.min,     SpringGeneticConstraints::frequency.max,      "Frequency = %.5f", CommandType::SetSpringFrequency },
        { s.genome.offset,         SpringGeneticConstraints::offset.min,        SpringGeneticConstraints::offset.max,         "Phase     = %.3f", CommandType::SetSpringOffset },
        { s.genome.vertical_shift, SpringGeneticConstraints::vertical_shift.min, SpringGeneticConstraints::vertical_shift.max, "Shift     = %.3f", CommandType::SetSpringVerticalShift });
}