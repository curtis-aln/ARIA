#include "debug_tab.h"
#include <entities/matter/matter_settings.h>
#include <imgui.h>
#include <managers/cell_manager/cell_manager_settings.h>
#include <simulation/context/sim_command.h>
#include <simulation/context/sim_snapshot.h>
#include <simulation/context/state.h>
#include <world/world_settings.h>

#include "../../../settings/settings.h" 
#include "../../../../Utils/Graphics/post_fx.h"

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

	ImGui::EndChild();

	ImGui::BeginChild("Reproduction", { cw, ch }, true);

	ImGui::SetNextItemWidth(-1.f);

	// Min speed
	bool use_min_speed = ctx.cell_toggles.min_speed_tax;
	toggle(snap, ctx, "min speed tax", &CellManagerToggles::min_speed_tax);

	if (use_min_speed)
	{
		ImGui::Indent();
		float min_speed = snap.cell_manager_stats.min_speed; // sync with sim state
		if (ImGui::SliderFloat("##min_speed", &min_speed, 0.f, 10.f, "Min Speed %.4f"))
		{
			SimCommand cmd{ .section = CommandSection::CellManagerEvent, .type = CommandType::SetMinSpeed, .float_val = min_speed };
			ctx.push(cmd);
		}
		
		float m_delta_min_speed_ = CellManagerSettings::delta_min_speed;
		float micro = m_delta_min_speed_ * 1e6f;
		ImGui::SetNextItemWidth(-1.f);
		if (ImGui::SliderFloat("##delta_min_speed", &micro, 0.f, 100.f, "Delta Min Speed %.1f e-6"))
		{
			m_delta_min_speed_ = micro * 1e-6f;
			SimCommand cmd{ .section = CommandSection::CellManagerEvent, .type = CommandType::SetDeltaMinSpeed, .float_val = m_delta_min_speed_ };
			ctx.push(cmd);
		}
		ImGui::Unindent();
	}

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
		if (ImGui::SliderFloat("##impulse_damage_multiplier", &damage, 0.f, 0.02f, "multiplier %.4f"))
			ctx.push({ .section = CommandSection::CellManagerEvent, .type = CommandType::SetImpulseDamageMultiplier, .float_val = damage });
		float max = CellManagerSettings::max_single_hit_integrity_fraction;
		if (ImGui::SliderFloat("##max_single_hit_integrity_fraction", &max, 0.f, 1.f, "max single hit integrity fraction %.2f"))
			ctx.push({ .section = CommandSection::CellManagerEvent, .type = CommandType::SetMaxImpulseDamage, .float_val = max });
		ImGui::Unindent();
	}

	const auto& cms = snap.cell_manager_stats;
	constexpr auto sec = CommandSection::CellManagerEvent;

	bool stress_integrity_damage = ctx.cell_toggles.tendon_stress_integrity_damage;
	toggle(snap, ctx, "spring stress integrity damage", &CellManagerToggles::tendon_stress_integrity_damage);
	if (stress_integrity_damage)
	{
		ImGui::Indent();
		float damage_thresh = SpringSettings::spring_damage_threshold;
		synced_slider(ctx, "##Damage Threshold", damage_thresh, cms.tendon_damage_threshold,
			0.f, 1000.f, "Damage Threshold %.2f", sec, CommandType::SetTendonDamageThreshold,
			"Tension above which a spring starts to take damage.");
		ImGui::Unindent();
	}
	
	bool too_long_breakage = ctx.cell_toggles.tendon_too_long_breakage;
	toggle(snap, ctx, "spring too long breakage", &CellManagerToggles::tendon_too_long_breakage);
	if (too_long_breakage)
	{
		ImGui::Indent();
		float break_thresh = SpringSettings::spring_break_length_factor;
		synced_slider(ctx, "##breaking Length ratio", break_thresh, cms.tendon_breaking_length,
			0.f, 2.f, "breaking Length %.2f", sec, CommandType::SetTendonBreakingLength,
			"How far a spring can stretch, relative to its rest length, before it snaps.");
		ImGui::Unindent();
	}


	bool too_much_force_breakage = ctx.cell_toggles.tendon_too_much_force_breakage;
	toggle(snap, ctx, "spring too much force breakage", &CellManagerToggles::tendon_too_much_force_breakage);
	if (too_much_force_breakage)
	{
		ImGui::Indent();
		float break_force = SpringSettings::spring_break_force;
		synced_slider(ctx, "##breaking Force", break_force, cms.tendon_breaking_force,
			0.f, 1000.f, "breaking Force %.2f", sec, CommandType::SetTendonBreakingForce,
			"How much force a spring can withstand before it snaps.");
		ImGui::Unindent();
	}

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
	if (ImGui::SliderInt("##cell_matter_lifetime", &lifetime, 0, 120000, "cell matter lifetime %d"))
		ctx.push({ .section = CommandSection::CellManagerEvent, .type = CommandType::SetCellMatterLifetime, .int_val = lifetime });
	
	

	ImGui::Spacing();
	toggle(snap, ctx, "easy reproduction", &CellManagerToggles::easy_reproduction);
	if (ctx.cell_toggles.easy_reproduction)
	{
		ImGui::Indent();
		float thresh = CellManagerSettings::easy_natural_birth_ratio;
		if (ImGui::SliderFloat("##easy_natural_birth_ratio", &thresh, 0.f, 1.f, "threshold %.2f"))
			ctx.push({ .section = CommandSection::CellManagerEvent, .type = CommandType::SetEasyNaturalBirthRatio, .float_val = thresh });
		ImGui::Unindent();
	}

	ImGui::Spacing();
	toggle(snap, ctx, "prune cells", &CellManagerToggles::prune_cells);

	ImGui::Separator();

	ImGui::EndChild();
	ImGui::SameLine();

	ImGui::BeginChild("Background", { cw, ch }, true);

	ImGui::TextDisabled("Background");
	ImGui::Separator();


	draw_post_fx_ui();

	ImGui::EndChild();
}