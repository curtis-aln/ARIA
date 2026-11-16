#include "food_tab.h"
#include <imgui.h>
#include <managers/food_manager/food_manager_settings.h>
#include <simulation/context/sim_command.h>
#include <simulation/context/sim_snapshot.h>
#include <simulation/context/state.h>

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

	// Food vibration
	if (ImGui::Checkbox("food vibration", &ctx.food_toggles.food_vibration))
		ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetFoodToggles, .food_toggles = ctx.food_toggles });

	if (ctx.food_toggles.food_vibration)
	{
		ImGui::Indent();
		float vibration_strength = FoodManagerSettings::vibration_strength;
		if (ImGui::SliderFloat("##vibration_strength", &vibration_strength, 0.f, 400.f, "vibration strength %.1f"))
			ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetVibrationStrength, .float_val = vibration_strength });
		
		int vibration_freq = FoodManagerSettings::vibration_frequency;
		if (ImGui::SliderInt("##vibration_frequency", &vibration_freq, 1, 250, "vibrate every N %d"))
			ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetVibrationFrequency, .int_val = vibration_freq });
		ImGui::Unindent();
	}

	// Food launch offspring & Launch chance & strength
	if (ImGui::Checkbox("food launch offspring", &ctx.food_toggles.food_launch_offspring))
		ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetFoodToggles, .food_toggles = ctx.food_toggles });

	if (ctx.food_toggles.food_launch_offspring)
	{
		ImGui::Indent();
		float launch_strength = FoodManagerSettings::food_launch_strength;
		if (ImGui::SliderFloat("##launch_strength", &launch_strength, 0.f, 2000.f, "launch strength %.1f"))
			ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetLaunchStrength, .float_val = launch_strength });

		float launch_chance = FoodManagerSettings::food_launch_chance * 100;
		if (ImGui::SliderFloat("##launch_chance", &launch_chance, 0.f, 100.f, "launch chance %.0f%%"))
			ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetLaunchChance, .float_val = launch_chance / 100});
		ImGui::Unindent();
	}

	// Death age and Death chance per iteration sliders
	int death_age = FoodSettings::death_age;
	if (ImGui::SliderInt("##death_age", &death_age, 0, 10000, "death age %d"))
		ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetFoodDeathAge, .int_val = death_age });

	float death_chance = FoodManagerSettings::death_age_chance * 100;
	if (ImGui::SliderFloat("##death_chance", &death_chance, 0.f, 5.f, "death chance %.2f%%"))
		ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetFoodDeathChance, .float_val = death_chance / 100 });

	// Max Food Count slider
	int max_food_count = FoodManagerSettings::max_food;
	if (ImGui::SliderInt("##max_food_count", &max_food_count, 1, 100000, "max food count %d"))
		ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetMaxFoodCount, .int_val = max_food_count });

	// Nutrient Reproductive Threshold slider
	float nutrient_repro_thresh = FoodSettings::nutrient_reproductive_threshold;
	if (ImGui::SliderFloat("##nutrient_repro_thresh", &nutrient_repro_thresh, 0.f, 1.f, "nutrient repro thresh %.2f"))
		ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetNutrientReproThresh, .float_val = nutrient_repro_thresh });

	// Development Time slider
	int development_time = FoodSettings::nutrient_development_time;
	if (ImGui::SliderInt("##development_time", &development_time, 0, 4000, "development time %d"))
		ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetDevelopmentTime, .int_val = development_time });

	// Reproductive Cooldown slider
	int repro_cooldown = FoodSettings::repro_cooldown;
	if (ImGui::SliderInt("##repro_cooldown", &repro_cooldown, 0, 1000, "reproductive cooldown %d"))
		ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetReproCooldown, .int_val = repro_cooldown });

	// Food Spawn Distance slider
	float food_spawn_dist = FoodSettings::food_offspring_spawn_distance;
	if (ImGui::SliderFloat("##food_spawn_dist", &food_spawn_dist, 0.f, 500.f, "food spawn distance %.1f"))
		ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetFoodSpawnDist, .float_val = food_spawn_dist });

	// Nutrient to Radius slider
	float nutrients_to_radius = FoodSettings::nutrients_to_mass_scale;
	if (ImGui::SliderFloat("##nutrients_to_radius", &nutrients_to_radius, 0.f, 4.f, "nutrients to radius %.2f"))
		ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetNutrientToRadius, .float_val = nutrients_to_radius });

	// Drag Coefficient slider
	float drag_coefficient = FoodManagerSettings::kDragLinearCoeff;
	if (ImGui::SliderFloat("##drag_coefficient", &drag_coefficient, 0.f, 0.1f, "drag coefficient %.3f"))
		ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetDragCoefficient, .float_val = drag_coefficient });

	// Initial Nutrients slider
	float initial_nutrients = FoodSettings::initial_nutrients;
	if (ImGui::SliderFloat("##initial_nutrients", &initial_nutrients, 0.f, 300.f, "initial nutrients %.2f"))
		ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetInitialNutrients, .float_val = initial_nutrients });

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
	if (ImGui::SliderInt("##update_freq", &update_freq, 1, 30, "Update Frequency %d"))
		ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetPheromoneUpdateFrequency, .int_val = update_freq });

	int  render_freq = FoodManagerSettings::pheromone_render_freq;
	if (ImGui::SliderInt("##render_freq", &render_freq, 1, 30, "Render Frequency %d"))
		ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetPheromoneRenderFrequency, .int_val = render_freq });


	// set the max nutrients
	if (ImGui::SliderFloat("##max_nutrients", &FoodSettings::final_nutrients, 10.f, 300.f, "Max Nutrients %.2f"))
		ctx.push({ .section = CommandSection::FoodManagerEvent, .type = CommandType::SetMaxNutrients, .float_val = FoodSettings::final_nutrients });



	ImGui::EndChild();
}