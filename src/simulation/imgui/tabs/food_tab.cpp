#include "food_tab.h"
#include <imgui.h>
#include <managers/food_manager/food_manager_settings.h>
#include <simulation/context/sim_command.h>
#include <simulation/context/sim_snapshot.h>
#include <simulation/context/state.h>
#include <Utils/Graphics/pheromone_grid.h>

void FoodTab::draw(const SimSnapshot& snap, ImGuiContext& ctx)
{
	const float total = ImGui::GetContentRegionAvail().x;
	const float sp = ImGui::GetStyle().ItemSpacing.x;
	const float cw = (total - sp * 2.f) / 3.f;
	const float ch = -1.f;

	ImGui::BeginChild("Food", { cw, ch }, true);

	ImGui::TextDisabled("Food");
	ImGui::Separator();

	if (ImGui::Checkbox("spawn random food", &ctx.food_toggles.spawn_random_food))
		ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetFoodToggles, .food_toggles = ctx.food_toggles });

	if (ctx.food_toggles.spawn_random_food)
	{
		ImGui::Indent();
		int spawns_per_frame = FoodManagerSettings::food_random_spawn_per_frame;
		if (ImGui::SliderInt("##random_intensity", &spawns_per_frame, 0, 10, "spawns per frame %d"))
			ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetRandomSpawnsPerFrame, .int_val = spawns_per_frame });

		float spawn_chance = FoodManagerSettings::food_random_spawn_chance;
		if (ImGui::SliderFloat("##random_chance", &spawn_chance, 0.f, 1.f, "spawn chance %.2f"))
			ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetRandomSpawnChance, .float_val = spawn_chance });

		ImGui::Unindent();
	}

	ImGui::Separator();

	if (ImGui::Checkbox("food mitosis", &ctx.food_toggles.food_mitosis))
		ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetFoodToggles, .food_toggles = ctx.food_toggles });

	if (ctx.food_toggles.food_mitosis)
	{
		ImGui::Indent();
		float constant = FoodManagerSettings::spawn_proportionality_constant;
		if (ImGui::SliderFloat("##mitosis_intensity", &constant, 0.f, .01f, "spawn constant %.4f"))
			ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetMitosisConstant, .float_val = constant });
		ImGui::Unindent();
	}

	ImGui::EndChild();
	ImGui::SameLine();

	// ----------------------------------------------

	ImGui::BeginChild("Pheromones", { cw, ch }, true);

	ImGui::TextDisabled("Pheromones");
	ImGui::Separator();
	ImGui::Spacing();

	// two toggles
	toggle(snap, ctx, "update pheromone grid", &FoodToggles::update_pheromone_grid);
	toggle(snap, ctx, "render pheromone grid", &FoodToggles::render_pheromone_grid);

	// int slider for updating steps 1 - 5
	int updating_steps = PheromoneGridSettings::substeps;
	if (ImGui::SliderInt("##updating_steps", &updating_steps, 1, 5, "Updating Steps %d"))
		ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetPheromoneUpdatingSteps, .int_val = updating_steps });

	// float sliders for decay, diffusion
	float decay_rate = PheromoneGridSettings::decay_rate;
	if (ImGui::SliderFloat("##decay_rate", &decay_rate, 0.0001f, 0.01f, "Decay Rate %.4f"))
		ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetPheromoneDecayRate, .float_val = decay_rate });

	float diffuse_rate = PheromoneGridSettings::diffuse_rate;
	if (ImGui::SliderFloat("##diffuse_rate", &diffuse_rate, 0.f, 1.f, "Diffuse Rate %.2f"))
		ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetPheromoneDiffusionRate, .float_val = diffuse_rate });

	// float slider for deposit amount and max pheromone level
	float deposit_amount = PheromoneGridSettings::deposit_amount;
	if (ImGui::SliderFloat("##deposit_amount", &deposit_amount, 0.f, 50.f, "Deposit Amount %.2f"))
		ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetPheromoneDepositAmount, .float_val = deposit_amount });

	float max_pheromone = PheromoneGridSettings::max_pheromone;
	if (ImGui::SliderFloat("##max_pheromone", &max_pheromone, 20.f, 200.f, "Max Pheromone %.2f"))
		ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetPheromoneMaxPheromone, .float_val = max_pheromone });

	ImGui::Separator();
	ImGui::Spacing();
	int update_freq = FoodManagerSettings::pheromone_update_freq;
	if (ImGui::SliderInt("##update_freq", &update_freq, 1, 120, "Update Frequency %d"))
		ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetPheromoneUpdateFrequency, .int_val = update_freq });

	int  render_freq = FoodManagerSettings::pheromone_render_update_freq;
	if (ImGui::SliderInt("##render_freq", &render_freq, 1, 120, "Render Frequency %d"))
		ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetPheromoneRenderFrequency, .int_val = render_freq });

	ImGui::EndChild();
}