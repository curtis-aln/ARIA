#include "organism_tab.h"
#include <algorithm>
#include <cstdio>
#include <entities/body/body.h>
#include <entities/cell/cell.h>
#include <entities/genome/cell_genome.h>
#include <entities/cell/cell_settings.h>
#include <imgui.h>
#include <SFML/System/Vector2.hpp>
#include <simulation/context/sim_command.h>
#include <simulation/imgui/tabs/i_tab.h>
#include <vector>

void OrganismTab::draw_cell_detail(ImGuiContext& ctx, const Cell& c, const Body& body)
{
	using G = CellGenome;

	const float frequency = c.get<G::Frequency>();
	const float amplitude = c.get<G::Amplitude>();
	const float vertical_shift = c.get<G::VerticalShift>();
	const float offset = c.get<G::Offset>();

	const int period = safe_time_period(frequency, k_max_wave_buf);
	float wave_min, wave_max;
	wave_range(amplitude, vertical_shift, 0.f, 1.f, wave_min, wave_max);

	const float current_friction = c.calculate_sin_output();

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
			draw_cell_detail_body_tab(ctx, c, body);
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::EndChild();
	ImGui::SameLine();

	// ── Sin wave ───────────────────────────────────────────────────────────
	const Range amp_lim = c.limits<G::Amplitude>();
	const Range freq_lim = c.limits<G::Frequency>();
	const Range offset_lim = c.limits<G::Offset>();
	const Range shift_lim = c.limits<G::VerticalShift>();

	draw_wave_panel(ctx, "CL_wave",
		"Friction  amplitude * sin(frequency * t + phase) + shift",
		static_cast<double>(c.internal_clock_), c.body_id_, "Friction", current_friction, friction_wave_col,
		{ amplitude,      amp_lim.min,    amp_lim.max,    "Amplitude = %.3f", CommandType::SetAmplitude },
		{ frequency,      freq_lim.min,   freq_lim.max,   "Frequency = %.5f", CommandType::SetFrequency },
		{ offset,         offset_lim.min, offset_lim.max, "Phase     = %.3f", CommandType::SetOffset },
		{ vertical_shift, shift_lim.min,  shift_lim.max,  "Shift     = %.3f", CommandType::SetVerticalShift });
}

// ── Cell detail: "Cell" sub-tab — biology / genetics ────────────────────────
void OrganismTab::draw_cell_detail_cell_tab(const Cell& c, const int period,
	const float wave_min, const float wave_max, const float /*current_friction*/)
{
	if (c.immortal_)
		ImGui::TextColored({ 0.3f, 0.8f, 0.3f, 1.f }, "Immortal");

	ImGui::Text("Absolute id %u  Gen %u", c.id_, c.generation());
	ImGui::Text("age %u fr", c.internal_clock_);
	ImGui::Text("Ate      %u  (%u fr ago)", static_cast<unsigned>(c.total_food_eaten_), static_cast<unsigned>(c.time_since_last_ate_));
	ImGui::Text("Offspring so far: %d", static_cast<int>(c.offspring_count));

	ImGui::Spacing();
	ImGui::Text("Spring Damage %.2f", c.cumulative_tendon_damage_);
	ImGui::Text("Collision Damage %.2f", c.cumulative_collision_damage_);
	ImGui::Text("delta energy: %.2f", c.delta_energy);
	ImGui::Text("delta integrity: %.2f", c.delta_integrity);

	// Spring links: stored IDs are absolute, show the relative cell number too.
	const auto& ids = c.tendon_links_.connected_tendon_ids_;
	const uint32_t n = c.tendon_links_.size_;
	ImGui::Text("spring links: %u", n);
	for (uint32_t i = 0; i < n; ++i)
	{
		const int rel = relative_cell_index(ids[i]);
		if (rel >= 0) ImGui::BulletText("Cell %d  (abs %u)", rel, ids[i]);
		else          ImGui::BulletText("abs %u", ids[i]);
	}

	// Digest cooldown: mirrors the gate in Cell::eat().
	ImGui::Spacing();
	ImGui::TextDisabled("Digestion");
	if (CellSettings::digestive_time <= 0.f)
	{
		ImGui::TextDisabled("disabled (digestive_time = 0)");
	}
	else
	{
		const float remaining = std::max(0.f, CellSettings::digestive_time - static_cast<float>(c.time_since_last_ate_));
		const float done = 1.f - remaining / CellSettings::digestive_time;
		char lbl[32];
		if (remaining > 0.f)
			snprintf(lbl, sizeof lbl, "%.0f fr left", remaining);
		else
			snprintf(lbl, sizeof lbl, "%s", c.can_eat() ? "Ready" : "Full");
		const ImVec4 col = remaining > 0.f ? ImVec4{ 0.95f, 0.70f, 0.20f, 1.f } : ImVec4{ 0.30f, 0.75f, 0.30f, 1.f };
		colored_progress(done, col, lbl);
	}
}

// ── Cell detail: "Body" sub-tab — spatial / physical ────────────────────────
void OrganismTab::draw_cell_detail_body_tab(ImGuiContext& ctx, const Cell& c, const Body& body)
{
	// data fetching and calculating
	sf::Vector2f pos = body.get_pos();
	sf::Vector2f vel = body.get_vel();
	float mass = body.get_mass();
	float radius = body.get_radius();
	float speed = vel.length();


	ImGui::Text("Body id  %u", c.body_id_);
	ImGui::Text("Pos      (%.0f, %.0f)", pos.x, pos.y);
	ImGui::Text("Speed    %.3f", speed);
	ImGui::Spacing();
	ImGui::Text("Mass     %.3f", mass);
	ImGui::Text("Radius   %.1f", radius);
	ImGui::Spacing();
	ImGui::Text("Impulse   %.3f", body.impulse_);
	ImGui::Text("Nearby count %d", body.nearby_ids_size_);

	ImGui::Spacing();
	ImGui::SetNextItemWidth(-1.f);
	const Range& mass_lim = c.limits<CellGenome::Mass>();

	slider_float_cmd(ctx, "##mass", mass,
		mass_lim.min, mass_lim.max,
		"Mass = %.1f", CommandSection::CellManagerEvent, CommandType::SetRadius);
}