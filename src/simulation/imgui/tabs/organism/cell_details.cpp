#include "organism_tab.h"
#include <algorithm>
#include <cstdio>
#include <entities/cell/cell.h>
#include <entities/cell/cell_genome.h>
#include <entities/cell/cell_settings.h>
#include <imgui.h>
#include <SFML/System/Vector2.hpp>
#include <simulation/context/sim_command.h>
#include <simulation/imgui/tabs/i_tab.h>
#include <vector>

void OrganismTab::draw_cell_detail(ImGuiContext& ctx, const Cell& c, const sf::Vector2f& pos, const sf::Vector2f& vel)
{
	const float speed_sq = vel.lengthSquared();
	float speed = -1;
	if (speed_sq != 0)
		speed = vel.length();

	const int frames_alive = c.internal_clock_;
	const float current_friction = c.calculate_friction();

	const int period = safe_time_period(c.frequency, k_max_wave_buf);
	float wave_min, wave_max;
	wave_range(c.amplitude, c.vertical_shift, 0.f, 1.f, wave_min, wave_max);

	// ── Stats ─────────────────────────────────────────────────────────────
	ImGui::BeginChild("CL_stat", spring_cell_box_size, true);

	if (ImGui::BeginTabBar("##cell_detail_tabs"))
	{
		if (ImGui::BeginTabItem("Cell"))
		{
			draw_cell_detail_cell_tab(c, period, wave_min, wave_max, current_friction);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Body"))
		{
			draw_cell_detail_body_tab(ctx, c, pos, speed);
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::EndChild();
	ImGui::SameLine();

	// ── Sin wave ───────────────────────────────────────────────────────────
	static std::vector<float> fric_buf;
	draw_wave_panel(ctx, current_friction, "CL_wave",
		"Friction  amplitude * sin(frequency * t + phase) + shift",
		frames_alive, c.body_id_, "friction", fric_buf,
		{ c.amplitude,      CellGeneticConstraints::amplitude.min,      CellGeneticConstraints::amplitude.max,      "Amplitude = %.3f", CommandType::SetAmplitude },
		{ c.frequency,      CellGeneticConstraints::frequency.min,      CellGeneticConstraints::frequency.max,      "Frequency = %.5f", CommandType::SetFrequency },
		{ c.offset,         CellGeneticConstraints::offset.min,         CellGeneticConstraints::offset.max,         "Phase     = %.3f", CommandType::SetOffset },
		{ c.vertical_shift, CellGeneticConstraints::vertical_shift.min, CellGeneticConstraints::vertical_shift.max, "Shift     = %.3f", CommandType::SetVerticalShift });
}

// ── Cell detail: "Cell" sub-tab — biology / genetics ────────────────────────
void OrganismTab::draw_cell_detail_cell_tab(const Cell& c, const int period,
	const float wave_min, const float wave_max, const float current_friction)
{
	if (c.immortal_)
		ImGui::TextColored({ 0.3f, 0.8f, 0.3f, 1.f }, "Immortal");

	ImGui::Text("id %d  Gen %d", c.body_id_, c.generation);
	ImGui::Text("age %zu fr", c.internal_clock_);
	ImGui::Text("Period   %d fr", period);
	ImGui::Text("Fric     min %.3f  max %.3f", wave_min, wave_max);
	ImGui::Text("Mut R    %.4f  Rng %.4f", c.guassian_const, c.mutation_range);
	ImGui::Text("Ate      %d  (%zu fr ago)", c.total_food_eaten_, c.time_since_last_ate_);

	ImGui::Text("Spring Damage %.2f", c.cumulative_spring_damage_);
	ImGui::Text("Collision Damage %.2f", c.cumulative_collision_damage_);
	ImGui::Text("delta energy: %.2f", c.delta_energy);
	ImGui::Text("delta integrity: %.2f", c.delta_integrity);

	// showing data from the array, The connections to springs this cell has from SpringGenome spring_genome{};
	const auto& ids = c.spring_links_.connected_spring_ids_;
	const int size = c.spring_links_.size_;

	ImGui::Text("spring links: %zu", size);
	for (size_t i = 0; i < size; ++i)
	{
		ImGui::BulletText("[%zu] %u", i, ids[i]);
	}

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

	// reproduction settings
	ImGui::Spacing();
	ImGui::TextDisabled("Reproduction");
	ImGui::Separator();

	// Small helper: colored progress bar (green=met, red=not met) + numeric readout
	auto draw_requirement = [](const char* label, float progress, float current, float threshold, bool met)
		{
			const ImVec4 met_color = ImVec4(0.30f, 0.75f, 0.30f, 1.0f);
			const ImVec4 unmet_color = ImVec4(0.80f, 0.35f, 0.30f, 1.0f);

			ImGui::PushStyleColor(ImGuiCol_PlotHistogram, met ? met_color : unmet_color);
			ImGui::ProgressBar(progress, ImVec2(-1.f, 4.f), "");
			ImGui::PopStyleColor();

			ImGui::Text("%s: %.1f / %.1f required   [%s]", label, current, threshold, met ? "OK" : "waiting");
		};

	draw_requirement("Energy", c.energy_progress(), c.get_energy(), c.energy_threshold(), c.check_sufficient_energy());
	draw_requirement("Integrity", c.integrity_progress(), c.get_integrity(), c.integrity_threshold(), c.check_sufficient_integrity());
	draw_requirement("Nutrients", c.nutrients_progress(), c.nutrients_, c.nutrients_threshold(), c.check_sufficient_nutrients());

	ImGui::Spacing();

	// Cooldown is a countdown, not a stockpile, so show ticks remaining instead of "X/Y required"
	{
		const bool cooldown_ready = c.check_repro_cooldown();
		const ImVec4 met_color = ImVec4(0.30f, 0.75f, 0.30f, 1.0f);
		const ImVec4 unmet_color = ImVec4(0.80f, 0.35f, 0.30f, 1.0f);

		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, cooldown_ready ? met_color : unmet_color);
		ImGui::ProgressBar(c.cooldown_progress(), ImVec2(-1.f, 4.0f), "");
		ImGui::PopStyleColor();

		if (cooldown_ready)
			ImGui::Text("Cooldown: ready");
		else
			ImGui::Text("Cooldown: %d ticks remaining", static_cast<int>(c.repro_cooldown) - static_cast<int>(c.repro_timer_));
	}

	ImGui::Spacing();
	ImGui::Separator();

	// Overall readiness — mirrors the AND of all four checks in update_organics()
	const bool ready = c.should_reproduce();
	ImGui::TextColored(
		ready ? ImVec4(0.30f, 0.85f, 0.30f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
		ready ? "Ready to reproduce" : "Not ready to reproduce"
	);

	ImGui::Text("Offspring so far: %d", c.offspring_count);
}

// ── Cell detail: "Body" sub-tab — spatial / physical ────────────────────────
void OrganismTab::draw_cell_detail_body_tab(ImGuiContext& ctx, const Cell& c,
	const sf::Vector2f& pos, const float speed)
{
	ImGui::Text("ID       %d", c.body_id_);
	ImGui::Text("Pos      (%.0f, %.0f)", pos.x, pos.y);
	ImGui::Text("Speed    %.3f", speed);
	ImGui::Text("Radius   %.1f", c.radius);

	ImGui::Spacing();
	ImGui::SetNextItemWidth(-1.f);
	slider_float_cmd(ctx, "##rad_c", c.radius,
		CellGeneticConstraints::radius.min, CellGeneticConstraints::radius.max,
		"R = %.1f", CommandSection::CellManagerEvent, CommandType::SetRadius);
}
