#include "food_tab.h"
#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <type_traits>
#include <imgui.h>
#include <managers/food_manager/food_manager_settings.h>
#include <simulation/context/sim_command.h>
#include <simulation/context/sim_snapshot.h>
#include <simulation/context/state.h>

namespace
{
	constexpr float kLabelFraction = 0.40f; // share of a column's width given to row labels
	constexpr int   kHoldFrames = 8;     // frames to keep showing a just-edited value (see held_or)

	// Window-relative x where sliders start. Set by Column, read by row_label.
	// Every row in a column uses the same value, so indented rows still line up.
	float g_slider_x = 0.f;

	const ImVec4 kWarnColor{ 1.f, 0.45f, 0.3f, 1.f };

	// ── Small helpers ────────────────────────────────────────────────────────

	void tooltip(const char* tip)
	{
		if (tip && ImGui::IsItemHovered() && !ImGui::IsItemActive())
			ImGui::SetTooltip("%s", tip);
	}

	// A titled bordered child. Optional right-aligned readout in the header line.
	struct Column
	{
		Column(const char* title, const float w, const float h,
			const char* right_text = nullptr, const ImVec4& right_color = ImVec4())
		{
			ImGui::BeginChild(title, { w, h }, true);

			const float x0 = ImGui::GetCursorPosX();
			const float avail = ImGui::GetContentRegionAvail().x;
			g_slider_x = x0 + avail * kLabelFraction;

			ImGui::TextDisabled("%s", title);
			if (right_text)
			{
				const float tw = ImGui::CalcTextSize(right_text).x;
				ImGui::SameLine();
				ImGui::SetCursorPosX(x0 + avail - tw);
				ImGui::TextColored(right_color, "%s", right_text);
			}
			ImGui::Separator();
		}

		~Column() { ImGui::EndChild(); }

		Column(const Column&) = delete;
		Column& operator=(const Column&) = delete;
	};

	// Label on the left, then positions the cursor so the next widget fills the rest of the row.
	void row_label(const char* text, const char* tip)
	{
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(text);
		tooltip(tip);
		ImGui::SameLine(g_slider_x);
		ImGui::SetNextItemWidth(-FLT_MIN);
	}

	// The sim thread applies commands a little after the UI pushes them. If we re-read the
	// live setting right away, the slider would flicker back to the old value. So for a few
	// frames after an edit we keep showing the value the user just chose.
	// Must be called inside the row's PushID scope.
	template <typename T>
	T held_or(const T live)
	{
		ImGuiStorage* store = ImGui::GetStateStorage();
		if (ImGui::GetFrameCount() > store->GetInt(ImGui::GetID("hold_until"), 0))
			return live;

		if constexpr (std::is_floating_point_v<T>)
			return store->GetFloat(ImGui::GetID("hold_value"), live);
		else
			return static_cast<T>(store->GetInt(ImGui::GetID("hold_value"), static_cast<int>(live)));
	}

	template <typename T>
	void hold(const T value)
	{
		ImGuiStorage* store = ImGui::GetStateStorage();
		store->SetInt(ImGui::GetID("hold_until"), ImGui::GetFrameCount() + kHoldFrames);

		if constexpr (std::is_floating_point_v<T>)
			store->SetFloat(ImGui::GetID("hold_value"), value);
		else
			store->SetInt(ImGui::GetID("hold_value"), static_cast<int>(value));
	}

	// lo / hi / fmt are in DISPLAY units. `scale` converts live -> display (e.g. 100 for percent);
	// the command carries the unscaled value. Never writes the setting directly.
	void slider_float(ImGuiContext& ctx, const char* label, const float live,
		const float lo, float hi, const char* fmt, const CommandType type,
		const char* tip = nullptr, const ImGuiSliderFlags flags = 0, const float scale = 1.f)
	{
		hi = std::max(hi, lo);

		ImGui::PushID(static_cast<int>(type));
		row_label(label, tip);

		float v = held_or(live * scale);
		const bool changed = ImGui::SliderFloat("##v", &v, lo, hi, fmt, flags | ImGuiSliderFlags_AlwaysClamp);

		if (changed)
		{
			hold(v);
			SimCommand cmd;
			cmd.section = CommandSection::FoodManagerEvent;
			cmd.type = type;
			cmd.float_val = v / scale;
			ctx.push(cmd);
		}
		else if (ImGui::IsItemActive())
			hold(v);

		tooltip(tip);
		ImGui::PopID();
	}

	void slider_int(ImGuiContext& ctx, const char* label, const int live,
		const int lo, int hi, const char* fmt, const CommandType type,
		const char* tip = nullptr, const ImGuiSliderFlags flags = 0)
	{
		hi = std::max(hi, lo);

		ImGui::PushID(static_cast<int>(type));
		row_label(label, tip);

		int v = held_or(live);
		const bool changed = ImGui::SliderInt("##v", &v, lo, hi, fmt, flags | ImGuiSliderFlags_AlwaysClamp);

		if (changed)
		{
			hold(v);
			SimCommand cmd;
			cmd.section = CommandSection::FoodManagerEvent;
			cmd.type = type;
			cmd.int_val = v;
			ctx.push(cmd);
		}
		else if (ImGui::IsItemActive())
			hold(v);

		tooltip(tip);
		ImGui::PopID();
	}

	// Checkbox for a FoodToggles field. ctx.food_toggles is the UI-side copy, so the box
	// responds instantly; the full struct is pushed on change. Returns the current value.
	bool food_toggle(ImGuiContext& ctx, const char* label, bool FoodToggles::* field,
		const char* tip = nullptr)
	{
		bool v = ctx.food_toggles.*field;
		const bool changed = ImGui::Checkbox(label, &v);
		tooltip(tip);

		if (changed)
		{
			ctx.food_toggles.*field = v;
			SimCommand cmd;
			cmd.section = CommandSection::FoodManagerEvent;
			cmd.type = CommandType::SetFoodToggles;
			cmd.food_toggles = ctx.food_toggles;
			ctx.push(cmd);
		}
		return v;
	}

	// ── Columns ──────────────────────────────────────────────────────────────

	void draw_spawning(const SimSnapshot& snap, ImGuiContext& ctx, const float w, const float h)
	{
		const int max_food = static_cast<int>(FoodManagerSettings::max_food);
		const int count = snap.food_manager_stats.food_count;
		const float used = max_food > 0 ? static_cast<float>(count) / static_cast<float>(max_food) : 1.f;

		char readout[48];
		std::snprintf(readout, sizeof(readout), "food %d / %d", count, max_food);

		Column col("Spawning", w, h, readout, fraction_color(std::clamp(1.f - used, 0.f, 1.f)));

		if (food_toggle(ctx, "random spawn", &FoodToggles::spawn_random_food,
			"Spawn food at random positions."))
		{
			ImGui::Indent();
			slider_int(ctx, "per frame", FoodManagerSettings::food_random_spawn_per_frame,
				0, 10, "%d", CommandType::SetRandomSpawnsPerFrame);
			slider_float(ctx, "chance", FoodManagerSettings::food_random_spawn_chance,
				0.f, 1.f, "%.2f", CommandType::SetRandomSpawnChance);
			ImGui::Unindent();
		}

		if (food_toggle(ctx, "mitosis", &FoodToggles::food_mitosis,
			"Food reproduces on its own. Needs the Growth & Death settings to allow it."))
		{
			ImGui::Indent();
			slider_float(ctx, "constant", FoodManagerSettings::spawn_proportionality_constant,
				0.f, 0.01f, "%.4f", CommandType::SetMitosisConstant);
			slider_float(ctx, "child distance", FoodSettings::food_offspring_spawn_distance,
				0.f, 500.f, "%.0f", CommandType::SetFoodSpawnDist,
				"Offspring appear within this distance of the parent.");
			ImGui::Unindent();
		}

		slider_int(ctx, "max food", max_food, 1, 100000, "%d", CommandType::SetMaxFoodCount,
			"Maximum number of food items (log scale).", ImGuiSliderFlags_Logarithmic);
	}

	void draw_growth(ImGuiContext& ctx, const float w, const float h)
	{
		const float init = FoodSettings::initial_nutrients;
		const float fin = FoodSettings::final_nutrients;
		const float thr = FoodSettings::nutrient_reproductive_threshold;
		const int   dev = static_cast<int>(FoodSettings::nutrient_development_time);
		const int   death = static_cast<int>(FoodSettings::death_age);

		Column col("Growth & Death", w, h);

		// Reproducing costs `start` nutrients, and food that drops below `start` dies. So a parent
		// only survives if  threshold x max >= 2 x start.  Each slider's range is bounded by the
		// other two so that combination can't be dialled in.
		const float init_hi = std::max(1.f, thr * fin * 0.5f);
		const float fin_lo = std::clamp(2.f * init / std::max(thr, 0.01f), 10.f, 300.f);
		const float thr_lo = fin > 1e-3f ? std::clamp(2.f * init / fin, 0.f, 1.f) : 0.f;

		slider_float(ctx, "start nutrients", init, 1.f, init_hi, "%.1f", CommandType::SetInitialNutrients,
			"Nutrients a new food starts with, and the cost of reproducing.\n"
			"Max = threshold x max nutrients / 2, so parents survive reproducing.");

		slider_float(ctx, "max nutrients", fin, fin_lo, 300.f, "%.0f", CommandType::SetMaxNutrients,
			"Nutrients food grows to.\nMin = 2 x start nutrients / threshold.");

		slider_int(ctx, "growth time", dev, 1, std::min(4000, death - 1), "%d frames",
			CommandType::SetDevelopmentTime,
			"Frames to grow from start to max nutrients.\nMax is just under death age.");

		slider_float(ctx, "repro threshold", thr, thr_lo, 1.f, "%.2f", CommandType::SetNutrientReproThresh,
			"Fraction of max nutrients needed to reproduce.\nMin = 2 x start nutrients / max nutrients.");

		slider_int(ctx, "repro cooldown", static_cast<int>(FoodSettings::repro_cooldown), 0, 1000,
			"%d frames", CommandType::SetReproCooldown, "Frames between reproductions.");

		slider_int(ctx, "death age", death, dev + 1, 10000, "%d frames", CommandType::SetFoodDeathAge,
			"Age at which food starts to wither.\nMin is just above growth time.");

		slider_float(ctx, "death chance", FoodManagerSettings::death_age_chance,
			0.f, 5.f, "%.2f%%", CommandType::SetFoodDeathChance, nullptr, 0, 100.f);

		// Fallback warnings, only visible if the state is already invalid (e.g. set from elsewhere).
		if (thr * fin < 2.f * init - 1e-3f)
		{
			ImGui::TextColored(kWarnColor, "! parents die when reproducing");
			tooltip("Threshold x max nutrients must be at least 2 x start nutrients.");
		}
		if (death <= dev)
		{
			ImGui::TextColored(kWarnColor, "! dies before fully grown");
			tooltip("Death age must be greater than growth time, or food can never reproduce.");
		}
	}

	void draw_motion(ImGuiContext& ctx, const float w, const float h)
	{
		Column col("Motion", w, h);

		if (food_toggle(ctx, "vibration", &FoodToggles::food_vibration,
			"Jostles food with a random force."))
		{
			ImGui::Indent();
			slider_float(ctx, "strength", FoodManagerSettings::vibration_strength,
				0.f, 400.f, "%.0f", CommandType::SetVibrationStrength, "Maximum random force per axis.");
			slider_int(ctx, "every", static_cast<int>(FoodManagerSettings::vibration_frequency),
				1, 250, "%d frames", CommandType::SetVibrationFrequency,
				"Applied whenever the food's age is a multiple of this.");
			ImGui::Unindent();
		}

		if (food_toggle(ctx, "launch offspring", &FoodToggles::food_launch_offspring,
			"Newborns are sometimes given a random velocity."))
		{
			ImGui::Indent();
			slider_float(ctx, "strength", FoodManagerSettings::food_launch_strength,
				0.f, 2000.f, "%.0f", CommandType::SetLaunchStrength, "Maximum launch velocity per axis.");
			slider_float(ctx, "chance", FoodManagerSettings::food_launch_chance,
				0.f, 100.f, "%.0f%%", CommandType::SetLaunchChance,
				"Chance that a newborn is launched.", 0, 100.f);
			ImGui::Unindent();
		}

		slider_float(ctx, "drag", FoodManagerSettings::kDragLinearCoeff,
			0.f, 0.1f, "%.3f", CommandType::SetDragCoefficient);

		slider_float(ctx, "mass scale", FoodSettings::nutrients_to_mass_scale,
			0.1f, 4.f, "%.2f", CommandType::SetNutrientToRadius, "Food mass = nutrients x this.");
	}

	void draw_pheromones(ImGuiContext& ctx, const float w, const float h)
	{
		Column col("Pheromones", w, h);

		const bool update = food_toggle(ctx, "update grid", &FoodToggles::update_pheromone_grid);
		const bool render = food_toggle(ctx, "render grid", &FoodToggles::render_pheromone_grid);

		ImGui::BeginDisabled(!update);
		slider_int(ctx, "steps", static_cast<int>(PheromoneGridSettings::substeps),
			1, 5, "%d", CommandType::SetPheromoneUpdatingSteps);
		slider_float(ctx, "decay", PheromoneGridSettings::decay_rate,
			0.0001f, 0.01f, "%.4f", CommandType::SetPheromoneDecayRate, nullptr, ImGuiSliderFlags_Logarithmic);
		slider_float(ctx, "diffusion", PheromoneGridSettings::diffuse_rate,
			0.f, 1.f, "%.2f", CommandType::SetPheromoneDiffusionRate);
		slider_float(ctx, "deposit", PheromoneGridSettings::deposit_amount,
			0.f, 50.f, "%.1f", CommandType::SetPheromoneDepositAmount);
		slider_float(ctx, "max level", PheromoneGridSettings::max_pheromone,
			20.f, 200.f, "%.0f", CommandType::SetPheromoneMaxPheromone);
		slider_int(ctx, "update every", static_cast<int>(FoodManagerSettings::pheromone_update_freq),
			1, 30, "%d frames", CommandType::SetPheromoneUpdateFrequency);
		ImGui::EndDisabled();

		ImGui::BeginDisabled(!render);
		slider_int(ctx, "render every", static_cast<int>(FoodManagerSettings::pheromone_render_freq),
			1, 30, "%d frames", CommandType::SetPheromoneRenderFrequency);
		ImGui::EndDisabled();
	}
}

void FoodTab::draw(const SimSnapshot& snap, ImGuiContext& ctx)
{
	constexpr int kColumns = 4;

	const float total = ImGui::GetContentRegionAvail().x;
	const float sp = ImGui::GetStyle().ItemSpacing.x;
	const float cw = (total - sp * static_cast<float>(kColumns - 1)) / static_cast<float>(kColumns);
	const float ch = -1.f;

	draw_spawning(snap, ctx, cw, ch);
	ImGui::SameLine();
	draw_growth(ctx, cw, ch);
	ImGui::SameLine();
	draw_motion(ctx, cw, ch);
	ImGui::SameLine();
	draw_pheromones(ctx, cw, ch);
}