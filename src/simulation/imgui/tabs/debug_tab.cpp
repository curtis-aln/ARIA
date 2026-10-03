#include "debug_tab.h"
#include <entities/matter/matter_settings.h>
#include <imgui.h>
#include <managers/cell_manager/cell_manager_settings.h>
#include <simulation/context/sim_command.h>
#include <simulation/context/sim_snapshot.h>
#include <simulation/context/state.h>
#include <world/world_settings.h>

#include "../../settings/settings.h" 

#include <cstdint>
#include <SFML/Graphics/Color.hpp>

void DebugTab::draw(const SimSnapshot& snap, ImGuiContext& ctx)
{
	const float total = ImGui::GetContentRegionAvail().x;
	const float sp = ImGui::GetStyle().ItemSpacing.x;
	const float cw = (total - sp * 2.f) / 3.f;
	const float ch = -1.f;

	// ── Playback ──────────────────────────────────────────────────────────────
	ImGui::BeginChild("Reproduction", { cw, ch }, true);

	ImGui::TextDisabled("Rendering");
	ImGui::Separator();

	if (ImGui::Checkbox("show only newborns", &ctx.cell_toggles.show_only_newborns))
		ctx.push({ .section = CommandSection::CellManagerEvent, .type = CommandType::SetCellToggles, .cell_toggles = ctx.cell_toggles });

	if (ImGui::Checkbox("show grid", &ctx.cell_toggles.show_newborn_grid))
		ctx.push({ .section = CommandSection::CellManagerEvent, .type = CommandType::SetCellToggles, .cell_toggles = ctx.cell_toggles });


	ImGui::EndChild();

	ImGui::BeginChild("Reproduction", { cw, ch }, true);

	ImGui::SetNextItemWidth(-1.f);
	float min_speed = snap.cell_manager_stats.min_speed; // sync with sim state
	if (ImGui::SliderFloat("##min_speed", &min_speed, 0.f, 10.f, "Min Speed %.3f"))
	{
		SimCommand cmd{ .section = CommandSection::CellManagerEvent, .type = CommandType::SetMinSpeed, .float_val = min_speed };
		ctx.push(cmd);
	}
	ImGui::Text("delta min speed (DMS): %.3f", CellManagerSettings::delta_min_speed);
	ImGui::Spacing();
	ImGui::Separator();
	toggle(snap, ctx, "Impulse Damage", &CellManagerToggles::collision_integrity_damage_);
	if (ctx.cell_toggles.collision_integrity_damage_)
	{
		ImGui::Indent();
		float thresh = CellManagerSettings::impulse_damage_thresh;
		if (ImGui::SliderFloat("##impulse_damage_thresh", &thresh, 0.f, 100.f, "threshold %.2f"))
			ctx.push({ .section = CommandSection::CellManagerEvent, .type = CommandType::SetImpulseDamageThreshold, .float_val = thresh });
		float damage = CellManagerSettings::impulse_damage_multiplier;
		if (ImGui::SliderFloat("##impulse_damage_multiplier", &damage, 0.f, 0.002f, "multiplier %.5f"))
			ctx.push({ .section = CommandSection::CellManagerEvent, .type = CommandType::SetImpulseDamageMultiplier, .float_val = damage });
		float max = CellManagerSettings::max_single_hit_integrity_fraction;
		if (ImGui::SliderFloat("##max_single_hit_integrity_fraction", &max, 0.f, 1.f, "max single hit integrity fraction %.2f"))
			ctx.push({ .section = CommandSection::CellManagerEvent, .type = CommandType::SetMaxImpulseDamage, .float_val = max });
		ImGui::Unindent();
	}

	toggle(snap, ctx, "spring stress integrity damage", &CellManagerToggles::spring_stress_integrity_damage);
	toggle(snap, ctx, "spring too long breakage", &CellManagerToggles::spring_too_long_breakage);
	toggle(snap, ctx, "spring too much force breakage", &CellManagerToggles::spring_too_much_force_breakage);

	toggle(snap, ctx, "old age death", &CellManagerToggles::old_age_death);
	if (ctx.cell_toggles.old_age_death)
	{
		//ImGui::Indent();
		//float max_age = CellManagerSettings::max_time_to_live;
		//if (ImGui::SliderFloat("##max_time_to_live", &max_age, 0.f, 30000.f, "max age %.0f"))
		//	ctx.push({ .section = CommandSection::CellManagerEvent, .type = CommandType::SetCellMatterLifetime, .int_val = static_cast<int>(max_age) });
		//ImGui::Unindent();
	}

	ImGui::Spacing();
	ImGui::TextDisabled("Energy Tax");
	toggle(snap, ctx, "work done energy", &CellManagerToggles::work_done_energy);

	toggle(snap, ctx, "friction energy loss", &CellManagerToggles::friction_energy_loss);

	// Sets the Lifetime of cell matter
	int lifetime = CellMatterSettings::max_time_to_live;
	if (ImGui::SliderInt("##cell_matter_lifetime", &lifetime, 0, 30000, "cell matter lifetime %d"))
		ctx.push({ .section = CommandSection::CellManagerEvent, .type = CommandType::SetCellMatterLifetime, .int_val = lifetime });

	// Setting the amount of Sub-iterations the program does
	int sub_iterations = WorldSettings::tick_sim_multiplier;
	if (ImGui::SliderInt("##sub_iterations", &sub_iterations, 1, 100, "update sub-iterations %d"))
		ctx.push({ .section = CommandSection::WorldEvent, .type = CommandType::SetSubIterations, .int_val = sub_iterations });

	// rendering the background grid
	toggle(snap, ctx, "background grid", &WorldToggles::draw_background_grid);

	ImGui::EndChild();
	ImGui::SameLine();

	ImGui::BeginChild("Background", { cw, ch }, true);

	ImGui::TextDisabled("Background");
	ImGui::Separator();

	const auto& presets = SimulationSettings::bg_presets;
	static int selected_preset = SimulationSettings::bg_preset_index;
	static float top_col[3] = {
		SimulationSettings::bg_colors[0].r / 255.f,
		SimulationSettings::bg_colors[0].g / 255.f,
		SimulationSettings::bg_colors[0].b / 255.f };
	static float bot_col[3] = {
		SimulationSettings::bg_colors[1].r / 255.f,
		SimulationSettings::bg_colors[1].g / 255.f,
		SimulationSettings::bg_colors[1].b / 255.f };

	if (ImGui::BeginCombo("##bg_preset", presets[selected_preset].name.c_str()))
	{
		for (int i = 0; i < static_cast<int>(presets.size()); ++i)
		{
			const bool is_selected = (selected_preset == i);
			if (ImGui::Selectable(presets[i].name.c_str(), is_selected))
			{
				selected_preset = i;
				if (presets[i].name != "Custom")
					ctx.push({ .section = CommandSection::SimulationEvent, .type = CommandType::SetBackgroundPreset, .int_val = i });
			}
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	if (presets[selected_preset].name == "Custom")
	{
		ImGui::Spacing();
		bool changed = false;
		changed |= ImGui::ColorEdit3("Top", top_col);
		changed |= ImGui::ColorEdit3("Bottom", bot_col);

		if (changed)
		{
			SimCommand cmd{ .section = CommandSection::SimulationEvent, .type = CommandType::SetCustomBackground };
			cmd.color_val_a = sf::Color(
				static_cast<uint8_t>(top_col[0] * 255.f),
				static_cast<uint8_t>(top_col[1] * 255.f),
				static_cast<uint8_t>(top_col[2] * 255.f));
			cmd.color_val_b = sf::Color(
				static_cast<uint8_t>(bot_col[0] * 255.f),
				static_cast<uint8_t>(bot_col[1] * 255.f),
				static_cast<uint8_t>(bot_col[2] * 255.f));
			ctx.push(cmd);
		}
	}

	ImGui::EndChild();
}