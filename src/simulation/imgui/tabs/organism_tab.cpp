#include "organism_tab.h"
#include "../helpers/plot_utils.h"
#include "../helpers/confirm_button.h"
#include <imgui.h>
#include <algorithm>

// Visual dimensions for the per-cell mini bars in the Energy tab.
static constexpr float k_mini_cell_box_width = 130.f;
static constexpr float k_mini_bar_height = 8.f;

// Placeholder upper bound for cell nutrients until a hard cap is added to
// ProtozoaSettings. Used for progress-bar scaling only — not enforced here.
static constexpr float k_max_nutrients = 500.f;
static constexpr float k_summary_bar_height = 18.f;


// ─────────────────────────────────────────────────────────────────────────────
//  Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

// Hard cap on sinwave buffer to prevent OOM when frequency is near zero.
static constexpr int k_max_wave_buf = 2048;

// Design constants
inline static constexpr ImVec4 nutrients_bar_col = { 0.35f, 0.75f, 0.35f, 1.f };
inline static constexpr ImVec4 selector_color = { 0.2, 0.2, 0.8, 1.0 };
inline static constexpr ImVec2 spring_cell_box_size = { 275.f, -1.f };


// Horizontal progress bar with a colour override and an overlay string.
static void colored_bar(const float fraction, const ImVec4& color,
    const char* overlay, const ImVec2 size = { -1.f, 10.f })
{
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
    ImGui::ProgressBar(fraction, size, overlay);
    ImGui::PopStyleColor();
}


static void labeled_bar(const char* prefix, const float fraction,
    const ImVec4& color, const char* overlay)
{
    ImGui::TextDisabled("%s", prefix);
    ImGui::SameLine();
    colored_bar(fraction, color, overlay, { -1.f, k_summary_bar_height });
}

// Safe period in frames for a given frequency.
static int safe_time_period(const float frequency)
{
    if (std::abs(frequency) < 1e-6f) return 120;
    return std::clamp(static_cast<int>(1.f / std::abs(frequency)), 1, k_max_wave_buf);
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

// ─────────────────────────────────────────────────────────────────────────────
//  Top-level draw
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw(const SimSnapshot& snap, ImGuiContext& ctx)
{
    const OrganismTracker& protozoa = snap.protozoa_tracker;
    if (!snap.selected_a_cell)
    {
	    draw_no_selection(); 
    	return;
    }

    // The panel showing information about the protozoa as a whole
    ImGui::BeginChild("OV_panel", { 240.f, -1.f }, false);
    draw_overview(protozoa);
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("TAB_panel", { -1.f, -1.f }, false);
    if (!ImGui::BeginTabBar("##org_tabs")) { ImGui::EndChild(); return; }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 6.f, 2.f });
    if (ImGui::BeginTabItem("Cells & Springs")) { draw_cells_springs_tab(ctx, protozoa);        ImGui::EndTabItem(); }
    if (ImGui::BeginTabItem("Energy")) { draw_energy_tab(ctx, snap);           ImGui::EndTabItem(); }
    if (ImGui::BeginTabItem("Tuning & Controls")) { draw_tuning_controls_tab(ctx, snap); ImGui::EndTabItem(); }
    ImGui::PopStyleVar();

    ImGui::EndTabBar();
    ImGui::EndChild();
}

// ─────────────────────────────────────────────────────────────────────────────
//  No selection
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_no_selection()
{
    ImGui::Spacing();
    const auto msg = "No organism selected — click one in the world";
    ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(msg).x) * 0.5f);
    ImGui::TextDisabled("%s", msg);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Overview panel
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_overview(const OrganismTracker& protozoa)
{


    // ── Identity / Locomotion side-by-side ───────────────────────────────
    ImGui::Columns(2, nullptr, false);

    ImGui::TextDisabled("Identity");
    ImGui::Text("ID      %d", protozoa.id);
    ImGui::Text("Age     %u", protozoa.frames_alive);
    ImGui::Text("Cells   %d", protozoa.cell_count);
    ImGui::Text("Springs %d", protozoa.spring_count);
    ImGui::NextColumn();

    ImGui::TextDisabled("Locomotion");
    ImGui::Text("Speed %.4f", protozoa.speed);
    ImGui::Text("Vel X %.3f", protozoa.velocity.x);
    ImGui::Text("Vel Y %.3f", protozoa.velocity.y);
    ImGui::Spacing();
    ImGui::TextDisabled("Offspring");
    ImGui::Text("Count %d", protozoa.offspring_count);
    ImGui::Text("Food  %u", protozoa.total_food_eaten);

    ImGui::Columns(1);
    ImGui::Spacing();

    // ── Energy ───────────────────────────────────────────────────────────
    ImGui::TextDisabled("Energy");
    const float energy_f = std::clamp(protozoa.total_energy / protozoa.max_energy, 0.f, 1.f);
    char energy_lbl[32];
    snprintf(energy_lbl, sizeof(energy_lbl), "%.0f / %.0f", protozoa.total_energy, protozoa.max_energy);
    colored_progress(energy_f, fraction_color(energy_f), energy_lbl);
    ImGui::Text("Spring Work %.3f", protozoa.spring_total_work_done);

    // ── Nutrients ───────
    ImGui::Spacing();
    ImGui::TextDisabled("Nutrients");
    const float nutrients_f = std::clamp(protozoa.total_nutrients / protozoa.max_nutrients, 0.f, 1.f);
    char nutrients_lbl[32];
    snprintf(energy_lbl, sizeof(nutrients_lbl), "%.0f / %.0f", protozoa.total_nutrients, protozoa.max_nutrients);
    colored_progress(nutrients_f, nutrients_bar_col, nutrients_lbl);


    // ── Repro cooldown: counts down to zero, stays at zero when ready ─────
    ImGui::Spacing();
    ImGui::TextDisabled("Reproduction cooldown");
    const float cooldown = protozoa.reproduction_cooldown;
    const float elapsed = protozoa.time_since_last_reproduced;
    const float remaining_f = std::clamp(1.f - elapsed / cooldown, 0.f, 1.f);
    char repro_lbl[24];
    snprintf(repro_lbl, sizeof(repro_lbl), "%.0f", remaining_f * cooldown);
    colored_progress(remaining_f, { 0.6f, 0.4f, 0.7f, 1.f },
        remaining_f <= 0.f ? "Ready" : repro_lbl);

    ImGui::Text("%d cells ready to reproduce (%.1f%%)",
        protozoa.cells_ready_to_reproduce,
        protozoa.ready_to_reproduce_percentage);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Cells & Springs tab
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_cells_springs_tab(ImGuiContext& ctx, const OrganismTracker& protozoa)
{
    // fetching cell and spring container information
    const int cell_count = protozoa.cell_count;
	const int spring_count = protozoa.spring_count;

	// No need to show the list if both are empty
    if (cell_count == 0 && spring_count == 0) 
    { 
        ImGui::TextDisabled("No cells or springs."); 
        return; 
    }

	// Clamp selection indices to valid ranges, and switch selection type if the currently selected type is empty
    if (!cell_count == 0)
        m_sel_cell_idx_ = std::min(m_sel_cell_idx_, cell_count - 1);

    if (!spring_count == 0)
        m_sel_spring_idx_ = std::min(m_sel_spring_idx_, spring_count - 1);

	// If the currently selected type is empty, switch to the other type (if it's not empty)
    if (m_sel_is_spring_ && spring_count == 0)
        m_sel_is_spring_ = false;

    if (!m_sel_is_spring_ && cell_count == 0)
        m_sel_is_spring_ = true;

    // ── Unified selection list ────────────────────────────────────────────
    constexpr ImVec2 list_size = { 88.f, -1.f };
    ImGui::BeginChild("CS_list", list_size, true);

    
    for (int i = 0; i < cell_count; ++i)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, selector_color);
        ImGui::Text("●");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        char lbl[12]; snprintf(lbl, sizeof(lbl), "C%d", i);
        if (ImGui::Selectable(lbl, !m_sel_is_spring_ && m_sel_cell_idx_ == i))
        {
            m_sel_cell_idx_ = i;
            m_sel_is_spring_ = false;
        }
    }

    if (!spring_count == 0)
    {
        ImGui::Separator();
        for (int i = 0; i < static_cast<int>(spring_count); ++i)
        {
            const Spring& si = protozoa.springs[i];
            char lbl[32]; snprintf(lbl, sizeof(lbl), "%d->%d##sp%d", si.cell_A_id, si.cell_B_id, i);
            if (ImGui::Selectable(lbl, m_sel_is_spring_ && m_sel_spring_idx_ == i))
            {
                m_sel_spring_idx_ = i;
                m_sel_is_spring_ = true;
            }
            //if (si.broken) ImGui::PopStyleColor();
        }
    }

    ImGui::EndChild();
    ImGui::SameLine();

    if (!m_sel_is_spring_ && !cell_count == 0)
        draw_cell_detail(ctx, protozoa.cells[m_sel_cell_idx_]);

    else if (m_sel_is_spring_ && !spring_count == 0)
        draw_spring_detail(ctx, protozoa, protozoa.springs[m_sel_spring_idx_]);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Cell detail (stats + sinwave)
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_cell_detail(ImGuiContext& ctx, const Cell& c)
{
	const sf::Vector2f& pos = c.get_pos();
	const sf::Vector2f& vel = c.get_vel();
	const float speed = vel.length();
    const int frames_alive = c.frames_alive_;
    const float current_friction = c.calculate_friction();

    auto slider_float_cmd = [&](const char* label, float current, float min, float max,
        const char* fmt, CommandType type)
        {
            float val = current;
            if (ImGui::SliderFloat(label, &val, min, max, fmt))
            {
                SimCommand cmd;
                cmd.type = type;
                cmd.float_val = val;
				cmd.cell_spring_idx = c.id_;
                ctx.push(cmd);
            }
        };

    const int period = safe_time_period(c.frequency);
    const int display_size = std::min(m_wave_cycles_ * period, k_max_wave_buf);
    const int head = frames_alive % display_size;
    float wave_min, wave_max;
    wave_range(c.amplitude, c.vertical_shift, 0.f, 1.f, wave_min, wave_max);

    // ── Stats ─────────────────────────────────────────────────────────────
    ImGui::BeginChild("CL_stat", spring_cell_box_size, true);

    ImGui::TextDisabled("Cell %d  Gen %d", c.id_, c.generation);
    ImGui::Text("Pos      (%.0f, %.0f)", pos.x, pos.y);
    ImGui::Text("Speed    %.3f", speed);
    ImGui::Text("Radius   %.1f", c.radius);
    ImGui::Text("Period   %d fr", period);
    ImGui::Text("Fric     min %.3f  max %.3f", wave_min, wave_max);
    ImGui::Text("Mut R    %.4f  Rng %.4f", c.mutation_rate, c.mutation_range);
    ImGui::Text("Ate      %d  (%zu fr ago)", c.total_food_eaten_, c.time_since_last_ate_);

    ImGui::Text("reproduce: %d", c.reproduce);
	ImGui::Text("offspring idx: %d", c.offspring_index);
	ImGui::Text("connection idx: %d", c.connection_index);
	ImGui::Text("spring copy idx: %d", c.spring_to_copy_index);

    // Digest cooldown bar
    const float digest_remaining = std::max(0.f,
        CellSettings::digestive_time -
        static_cast<float>(c.time_since_last_ate_));
    const float digest_f = digest_remaining / CellSettings::digestive_time;
    char digest_lbl[32];
    snprintf(digest_lbl, sizeof(digest_lbl), "%.0f fr left", digest_remaining);
    ImGui::TextDisabled("Digest cooldown");
    colored_progress(digest_f, fraction_color(digest_f),
        digest_f <= 0.f ? "Ready" : digest_lbl);

    // Current friction
    const float fric = current_friction;
    const ImVec4 fc = { 1.f - fric, fric, 0.2f, 1.f };
    ImGui::Spacing();
    ImGui::TextDisabled("Friction");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, fc);
    ImGui::Text("%.4f", fric);
    ImGui::PopStyleColor();
    colored_progress(fric, fc, "", { -1.f, 5.f });

    // Color swatches + radius slider
    const sf::Color outer = c.get_outer_color();
	const sf::Color inner = c.get_inner_color();

    ImGui::Spacing();
    const ImVec4 oc = { outer.r / 255.f, outer.g / 255.f,
                        outer.b / 255.f, outer.a / 255.f };
    const ImVec4 ic = { inner.r / 255.f, inner.g / 255.f,
                        inner.b / 255.f, inner.a / 255.f };
    ImGui::ColorButton("##cout", oc, 0, { 26.f, 13.f }); ImGui::SameLine(); ImGui::Text("Out");
    ImGui::SameLine(0, 10.f);
    ImGui::ColorButton("##cin", ic, 0, { 26.f, 13.f }); ImGui::SameLine(); ImGui::Text("In");
    ImGui::Spacing();
    ImGui::SetNextItemWidth(-1.f);
    
    slider_float_cmd("##rad_c", c.radius,
        CellGeneticConstraints::radius.min, CellGeneticConstraints::radius.max,
        "R = %.1f", CommandType::SetRadius);

    ImGui::EndChild();
    ImGui::SameLine();

    // ── Sin wave ───────────────────────────────────────────────────────────
    ImGui::BeginChild("CL_wave", { -1.f, -1.f }, true);
    ImGui::TextDisabled("Friction  amplitude * sin(frequency * t + phase) + shift");

    static std::vector<float> fric_buf;
    fric_buf.resize(display_size);
    PlotUtils::fill_sinwave(fric_buf.data(), display_size,
        c.amplitude, c.frequency, c.offset, c.vertical_shift, 0.f, 1.f);
    PlotUtils::sinwave_graph("##fw", fric_buf.data(), display_size, head, 0.f, 1.f, { -1.f, 52.f });
    ImGui::Text("t=%-4d  friction = %.4f", head, fric_buf[head]);

    ImGui::Spacing();
    ImGui::SetNextItemWidth(-1.f);
    ImGui::SliderInt("##cycles_c", &m_wave_cycles_, 1, 8, "Display cycles = %d");

    ImGui::Spacing();
    ImGui::SetNextItemWidth(-1.f);
    slider_float_cmd("##cA", c.amplitude, CellGeneticConstraints::amplitude.min, CellGeneticConstraints::amplitude.max, "Amplitude = %.3f", CommandType::SetAmplitude);
    ImGui::SetNextItemWidth(-1.f);
	slider_float_cmd("##cB", c.frequency, CellGeneticConstraints::frequency.min, CellGeneticConstraints::frequency.max, "Frequency = %.5f", CommandType::SetFrequency);
    ImGui::SetNextItemWidth(-1.f);
	slider_float_cmd("##cC", c.offset, CellGeneticConstraints::offset.min, CellGeneticConstraints::offset.max, "Phase     = %.3f", CommandType::SetOffset);
    ImGui::SetNextItemWidth(-1.f);
	slider_float_cmd("##cD", c.vertical_shift, CellGeneticConstraints::vertical_shift.min, CellGeneticConstraints::vertical_shift.max, "Shift     = %.3f", CommandType::SetVerticalShift);

    ImGui::EndChild();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Spring detail (stats + sin wave)
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_spring_detail(ImGuiContext& ctx, const OrganismTracker& p, const Spring& s)
{
    auto slider_float_cmd = [&](const char* label, float current, float min, float max,
        const char* fmt, CommandType type)
        {
            float val = current;
            if (ImGui::SliderFloat(label, &val, min, max, fmt))
            {
                SimCommand cmd;
                cmd.type = type;
                cmd.float_val = val;
                cmd.cell_spring_idx = s.id;
                ctx.push(cmd);
            }
        };

    const int period = safe_time_period(s.frequency);
    const int display_size = std::min(m_wave_cycles_ * period, k_max_wave_buf);
    const int head = static_cast<int>(p.frames_alive) % display_size;
    float ext_min, ext_max;
    wave_range(s.amplitude, s.vertical_shift, 0.f, 1.f, ext_min, ext_max);


    // ── Stats ─────────────────────────────────────────────────────────────
    ImGui::BeginChild("SL_stat", spring_cell_box_size, true);

    ImGui::TextDisabled("Spring %d->%d", s.cell_A_id, s.cell_B_id);

    const float length_diff = s.rest_length - s.current_length;
    ImGui::Text("Rest L:  %.1f, Real L: %.1f", s.rest_length, s.current_length);
    ImGui::Text("Length Diff:  %.2f", length_diff);
    ImGui::Text("Period        %d frames", period);
    ImGui::Text("Extension min %.0f  max %.0f", ext_min, ext_max);
    ImGui::Text("Generation    %d", s.generation);
    ImGui::Text("Mutation R    %.4f  Rng %.4f", s.mutation_rate, s.mutation_range);

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

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, { 0.4f, 0.8f, 1.f, 1.f });
    ImGui::ProgressBar(std::clamp(total_force * force_scale, 0.f, 1.f), { -1.f, 8.f }, "");
    ImGui::SameLine(); ImGui::Text("Force  %.2f", total_force);

    ImGui::ProgressBar(std::clamp((s.current_length - ext_min) * ext_scale, 0.f, 1.f), { -1.f, 8.f }, "");
    ImGui::SameLine(); ImGui::Text("Ext    %.2f", s.current_length - s.rest_length);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::TextDisabled("Physical");
    ImGui::SetNextItemWidth(-1.f);

    slider_float_cmd("##sk", s.spring_const,
        0.f, SpringGeneticConstraints::spring_const.max,
        "Spring constant = %.3f", CommandType::SetSpringConst);

    ImGui::SetNextItemWidth(-1.f);
    slider_float_cmd("##sd", s.damping,
        0.f, SpringGeneticConstraints::damping.max,
        "Damping         = %.3f", CommandType::SetDampingConst);

    ImGui::EndChild();
    ImGui::SameLine();

    // ── Sin wave ───────────────────────────────────────────────────────────
    ImGui::BeginChild("SL_wave", { -1.f, -1.f }, true);
    ImGui::TextDisabled("Extension  amplitude * sin(frequency * t + phase) + shift  [0, 1]");

    static std::vector<float> ext_buf;
    ext_buf.resize(display_size);
    PlotUtils::fill_sinwave(ext_buf.data(), display_size,
        s.amplitude, s.frequency, s.offset, s.vertical_shift, 0.f, 1.f);
    PlotUtils::sinwave_graph("##sw", ext_buf.data(), display_size, head, 0.f, 1.f, { -1.f, 52.f });
    ImGui::Text("t=%-4d  ratio = %.4f", head, ext_buf[head]);

    ImGui::Spacing();
    ImGui::SetNextItemWidth(-1.f);
    ImGui::SliderInt("##cycles_s", &m_wave_cycles_, 1, 8, "Display cycles = %d");

    ImGui::Spacing();
    ImGui::SetNextItemWidth(-1.f);
    slider_float_cmd("##sA", s.amplitude,
        0.f, SpringGeneticConstraints::amplitude.max,
        "Amplitude = %.3f", CommandType::SetSpringAmplitude);

    ImGui::SetNextItemWidth(-1.f);
    slider_float_cmd("##sB", s.frequency,
        -SpringGeneticConstraints::frequency.min, SpringGeneticConstraints::frequency.max,
        "Frequency = %.5f", CommandType::SetSpringFrequency);

    ImGui::SetNextItemWidth(-1.f);
    slider_float_cmd("##sC", s.offset,
        -SpringGeneticConstraints::offset.min, SpringGeneticConstraints::offset.max,
        "Phase     = %.3f", CommandType::SetSpringOffset);

    ImGui::SetNextItemWidth(-1.f);
    slider_float_cmd("##sD", s.vertical_shift,
        -SpringGeneticConstraints::vertical_shift.min, SpringGeneticConstraints::vertical_shift.max,
        "Shift     = %.3f", CommandType::SetSpringVerticalShift);

    ImGui::EndChild();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tuning & Controls tab
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_tuning_controls_tab(ImGuiContext& ctx, const SimSnapshot& snap)
{
    const OrganismTracker& p = snap.protozoa_tracker;
    const float sp = ImGui::GetStyle().ItemSpacing.x;
    const float total = ImGui::GetContentRegionAvail().x;
    const float cw = (total - sp * 2.f) / 3.f;
    constexpr float ch = -1.f;

    // ── Column 1: Mutation & Structure ───────────────────────────────────
    ImGui::BeginChild("TC_mut", { cw, ch }, true);
    ImGui::TextDisabled("Mutation");
    ImGui::Separator();
    static float tun_rate = 0.2f, tun_range = 0.2f;
    ImGui::SetNextItemWidth(-1.f); ImGui::SliderFloat("##tr", &tun_rate, 0.f, 1.f, "Rate  = %.3f");
    ImGui::SetNextItemWidth(-1.f); ImGui::SliderFloat("##trng", &tun_range, 0.f, 1.f, "Range = %.3f");
    ImGui::Spacing();
    if (ImGui::Button("Apply Mutation", { -1.f, 0.f }))
    {
        SimCommand cmd{.type = CommandType::MutateProtozoa };
        cmd.mutate = {.mut_rate = tun_rate, .mut_range = tun_range };
        ctx.push(cmd);
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Structure");
    ImGui::Separator();
    ImGui::Columns(2, nullptr, false);
    if (ImGui::Button("Add Cell", { -1.f, 0.f })) ctx.push({.type = CommandType::AddCell });
    if (ImGui::Button("Remove Cell", { -1.f, 0.f })) ctx.push({.type = CommandType::RemoveCell });
    ImGui::NextColumn();
    if (ImGui::Button("Add Spring", { -1.f, 0.f })) ctx.push({.type = CommandType::AddSpring });
    if (ImGui::Button("Remove Spring", { -1.f, 0.f })) ctx.push({.type = CommandType::RemoveSpring });
    ImGui::Columns(1);

    ImGui::Spacing();
    ImGui::TextDisabled("Feed");
    ImGui::Separator();
    static float feed_energy = 50.f;
    ImGui::SetNextItemWidth(-70.f);
    ImGui::SliderFloat("##feed", &feed_energy, 1.f, 500.f, "%.0f energy");
    ImGui::SameLine();
    if (ImGui::Button("Inject##org"))
    {
        SimCommand cmd{.type = CommandType::InjectProtozoa };
        cmd.float_val = feed_energy;
        ctx.push(cmd);
    }
    ImGui::EndChild();
    ImGui::SameLine();

    // ── Column 2: Lifecycle ───────────────────────────────────────────────
    ImGui::BeginChild("TC_life", { cw, ch }, true);
    ImGui::TextDisabled("Lifecycle");
    ImGui::Separator();

    //bool immortal = p.immortal; // todo
    bool immortal = false;
    if (ImGui::Checkbox("Immortal##org", &immortal))
    {
        SimCommand cmd{.type = CommandType::MakeImmortal };
        cmd.bool_val = immortal;
        ctx.push(cmd);
    }
    ImGui::Spacing();
    if (ImGui::Button("Force Reproduce##org", { -1.f, 0.f }))
        ctx.push({.type = CommandType::ForceReproduce });

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, { 0.55f, 0.08f, 0.08f, 1.f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.75f, 0.15f, 0.15f, 1.f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 1.00f, 0.25f, 0.25f, 1.f });
    if (ConfirmButton::draw("Force Die##org", { -1.f, 0.f }))
        ctx.push({.type = CommandType::KillProtozoa });
    ImGui::PopStyleColor(3);
    ImGui::EndChild();
    ImGui::SameLine();

    // ── Column 3: Clone, File & Tag ───────────────────────────────────────
    ImGui::BeginChild("TC_clone", { -1.f, ch }, true);
    ImGui::TextDisabled("Clone & File");
    ImGui::Separator();
    if (ImGui::Button("Clone nearby##org", { -1.f, 0.f })) ctx.push({.type = CommandType::CloneProtozoa });
    if (ImGui::Button("Save to file (stub)##org", { -1.f, 0.f })) {}
    if (ImGui::Button("Load & spawn from file (stub)##org", { -1.f, 0.f })) {}

    ImGui::Spacing();
    ImGui::TextDisabled("Tag");
    ImGui::Separator();
    ImGui::TextDisabled("Use the Tagged tab to tag ID %d", p.id);
    ImGui::EndChild();
}


// ─────────────────────────────────────────────────────────────────────────────
//  Energy tab
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_energy_tab(ImGuiContext& ctx, const SimSnapshot& snap)
{
    const OrganismTracker& tracker = snap.protozoa_tracker;
    const auto& cells = tracker.cells;
    const int              n = tracker.cell_count;

    if (n == 0) { ImGui::TextDisabled("No cells."); return; }

    // ── Aggregate totals not pre-computed by tracker ───────────────────────
    float total_integrity = 0.f;
    for (const Cell& c : cells)
        total_integrity += c.integrity;

    const float org_integrity_max = CellSettings::integrity * static_cast<float>(n);
    const float org_nutrients_max = k_max_nutrients * static_cast<float>(n);

    // ── Set up 3 columns with explicit widths ─────────────────────────────
    const float total_w = ImGui::GetContentRegionAvail().x;
    ImGui::Columns(3, "energy_cols", false);
    ImGui::SetColumnWidth(0, total_w * 0.45f);
    ImGui::SetColumnWidth(1, total_w * 0.25f);
    // column 2 takes the rest

    // ═════════════════════════════════════════════════════════════════════
    //  COLUMN 0 — Organism summary bars + per-cell grid
    // ═════════════════════════════════════════════════════════════════════
    ImGui::TextDisabled("Organism Summary  (%d cells)", n);
    ImGui::Separator();
    ImGui::Spacing();

    // Energy — green = at reproduce threshold, red = depleted
    {
        const float f = std::clamp(tracker.total_energy / tracker.max_energy, 0.f, 1.f);
        char lbl[48]; snprintf(lbl, sizeof(lbl), "%.1f / %.0f", tracker.total_energy, tracker.max_energy);
        labeled_bar("NRG", f, fraction_color(f), lbl);
    }
    // Nutrients — blue
    {
        const float f = std::clamp(tracker.total_nutrients / org_nutrients_max, 0.f, 1.f);
        char lbl[48]; snprintf(lbl, sizeof(lbl), "%.1f / %.0f", tracker.total_nutrients, org_nutrients_max);
        labeled_bar("NUT", f, { 0.2f, 0.6f, 1.0f, 1.f }, lbl);
    }
    // Integrity — yellow
    {
        const float f = std::clamp(total_integrity / org_integrity_max, 0.f, 1.f);
        char lbl[48]; snprintf(lbl, sizeof(lbl), "%.1f / %.0f", total_integrity, org_integrity_max);
        labeled_bar("INT", f, { 0.85f, 0.8f, 0.25f, 1.f }, lbl);
    }

    ImGui::Spacing();
    ImGui::Text("Spring drain this frame:  %.5f", tracker.spring_total_work_done);
    ImGui::Spacing();
    ImGui::Spacing();

    // ── Per-cell mini bar grid ─────────────────────────────────────────────
    ImGui::TextDisabled("Per-Cell  [ E = energy   N = nutrients   I = integrity ]");
    ImGui::Spacing();

    const float sp = ImGui::GetStyle().ItemSpacing.x;
    const float avail_w = ImGui::GetContentRegionAvail().x;
    const int   cells_per_row = std::max(1,
        static_cast<int>((avail_w + sp) / (k_mini_cell_box_width + sp)));

    for (int i = 0; i < n; ++i)
    {
        const Cell& c = cells[i];

        if (i > 0 && i % cells_per_row != 0)
            ImGui::SameLine();

        ImGui::PushID(i);
        ImGui::BeginGroup();

        ImGui::TextDisabled("C%d", i);
        ImGui::SameLine();
        ImGui::TextDisabled("(x%u)", c.total_food_eaten_);

        // Energy bar
        {
            const float f = std::clamp(c.energy / CellSettings::reproduce_energy_thresh, 0.f, 1.f);
            ImGui::TextDisabled("E"); ImGui::SameLine(0.f, 3.f);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, fraction_color(f));
            ImGui::ProgressBar(f, { k_mini_cell_box_width, k_mini_bar_height }, "");
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Energy: %.2f  /  %.0f (repro thresh)",
                    c.energy, CellSettings::reproduce_energy_thresh);
        }
        // Nutrients bar
        {
            const float f = std::clamp(c.nutrients_ / k_max_nutrients, 0.f, 1.f);
            ImGui::TextDisabled("N"); ImGui::SameLine(0.f, 3.f);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4{ 0.2f, 0.6f, 1.0f, 1.f });
            ImGui::ProgressBar(f, { k_mini_cell_box_width, k_mini_bar_height }, "");
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Nutrients: %.2f  /  %.0f (cap)", c.nutrients_, k_max_nutrients);
        }
        // Integrity bar
        {
            const float f = std::clamp(c.integrity / CellSettings::integrity, 0.f, 1.f);
            ImGui::TextDisabled("I"); ImGui::SameLine(0.f, 3.f);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4{ 0.85f, 0.8f, 0.25f, 1.f });
            ImGui::ProgressBar(f, { k_mini_cell_box_width, k_mini_bar_height }, "");
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Integrity: %.2f  /  %.0f", c.integrity, CellSettings::integrity);
        }

        ImGui::EndGroup();
        ImGui::PopID();
    }

    // ═════════════════════════════════════════════════════════════════════
    //  COLUMN 1 — Feed controls
    // ═════════════════════════════════════════════════════════════════════
    ImGui::NextColumn();

    ImGui::BeginChild("EN_feed", { -1.f, -1.f }, true);
    ImGui::TextDisabled("Feed");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::RadioButton("Energy##fmode", &m_feed_mode_, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Nutrients##fmode", &m_feed_mode_, 1);

    ImGui::Spacing();
    ImGui::SetNextItemWidth(-1.f);
    ImGui::SliderFloat("##feed_amount", &m_feed_amount_, 1.f, 500.f, "Amount = %.0f");

    ImGui::Spacing();
    if (ImGui::Button("Inject##en_inject", { -1.f, 0.f }))
    {
        // Distributed evenly across all cells via the command handler.
        //const CommandType type = (m_feed_mode_ == 0)
        //    ? CommandType::InjectProtozoa
        //    : CommandType::InjectNutrients;

        //SimCommand cmd{ type };
        //cmd.float_val = m_feed_amount_;
        //ctx.push(cmd);
    }
    ImGui::EndChild();

    // ═════════════════════════════════════════════════════════════════════
    //  COLUMN 2 — Conversion constants
    // ═════════════════════════════════════════════════════════════════════
    ImGui::NextColumn();

    ImGui::BeginChild("EN_const", { -1.f, -1.f }, true);
    ImGui::TextDisabled("Conversion rates  (per frame, per cell)");
    ImGui::Separator();
    ImGui::Text("Energy decay             %.5f", CellSettings::energy_decay_rate);
    ImGui::Text("Nutrients  ->  Energy    %.5f", CellSettings::conversion_rate);
    ImGui::Text("Energy     ->  Integrity %.5f", CellSettings::integrity_conversion_rate);
    ImGui::Text("Energy share (springs)   %.5f", SpringSettings::energy_share_rate);
    ImGui::Text("Spring work cost         %.5f", SpringSettings::spring_work_const);

    ImGui::Spacing();
    ImGui::TextDisabled("Thresholds  &  costs");
    ImGui::Separator();
    ImGui::Text("Initial energy           %.1f", CellSettings::initial_energy);
    ImGui::Text("Reproduce threshold      %.1f", CellSettings::reproduce_energy_thresh);
    ImGui::Text("Offspring energy cost    %.1f", CellSettings::offspring_energy_cost);
    ImGui::Text("Integrity max            %.1f", CellSettings::integrity);
    ImGui::Text("Nutrients cap (*)        %.1f", k_max_nutrients);

    ImGui::Spacing();
    ImGui::TextDisabled("(*) placeholder — replace once ProtozoaSettings::max_nutrients exists");
    ImGui::EndChild();

    // ── Reset columns ──────────────────────────────────────────────────────
    ImGui::Columns(1);
}