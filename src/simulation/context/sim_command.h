#pragma once
#include "state.h"

#include <mutex>
#include <queue>
#include <SFML/Graphics/Color.hpp>
#include <utility>

enum class CommandSection
{
	SimulationEvent,  // Events related to simulation settings and controls
	WorldEvent,       // Events related to world settings and controls
	CellManagerEvent, // Events related to cell management (protozoa)
	FoodManagerEvent  // Events related to food management
};

enum class CommandType
{
	// SIMULATION EVENT  ──────────────────────────────────────────────────
	SetSimToggles,
	SetUpdatingFrameRate,    // Sets the frame rate for the updating thread
	SetRenderingFrameRate,   // Sets the frame rate for the rendering thread
	SetMouseMode,            // The mouse mode (add, remove, attract, repel)
	SetZoomLevel,           // the zoom level slider
	SetToSimulationTab,
	SetToGraphsTab,
	SetToOrganismTab,
	SetToTaggedTab,
	SetToGridTab,
	SetToOVecDebugTab,

	// WORLD EVENT  ──────────────────────────────────────────────────
	SetWorldToggles,         // sets the world toggles
	ResetSimulation,         // resets the simulation to its initial state

	SetInfluenceRadius,      // set the influence radius of the mouse tool
	SetMouseIntensity,       // set the intensity of the mouse tool

	SetCellGridResolution,   // apply new cell grid resolution
	SetFoodGridResolution,

	SetWorldRadius,          // world resize stub
	SetSubIterations,

	SetBackgroundPreset,   // apply a preset by index (int_val)
	SetCustomBackground,   // apply a custom gradient (color_val_a/b)

	// CELL MANAGER EVENT  ──────────────────────────────────────────────────
	SetCellToggles,
	ClearAllProtozoa,       // clears all protozoa from the simulation

	SetMinSpeed,               // set the minimum speed of protozoa

	SetRadius,              // set radius of selected cell
	SetAmplitude,           // set amplitude of selected cell
	SetFrequency,           // set frequency of selected cell
	SetOffset,              // set offset of selected cell
	SetVerticalShift,       // set vertical shift of selected cell

	MutateProtozoa,         // mutate a specific protozoa (payload identifies protozoa and mutation parameters)
	AddCell,                // add a new cell to the selected protozoa
	RemoveCell,             // remove a cell from the selected protozoa
	AddSpring,              // add a spring between two cells of the selected protozoa
	RemoveSpring,           // remove a spring between two cells of the selected protozoa
	InjectProtozoa,         // inject a protozoa with Nutrients
	MakeImmortal,           // make the selected protozoa immortal (no death by age)
	ForceReproduce,         // force the selected protozoa to reproduce
	KillProtozoa,           // kill the selected protozoa (remove it from the simulation)
	CloneProtozoa,          // spawn a clone of the selected protozoa

	SetEasyNaturalBirthRatio, // set the easy natural birth ratio of the cell manager

	// Springs
	SetSpringConst,         // set the spring constant of a specific spring
	SetDampingConst,        // set the damping constant of a specific spring
	SetSpringAmplitude,     // set the spring amplitude of a specific spring
	SetSpringFrequency,     // set the spring frequency of a specific spring
	SetSpringOffset,        // set the spring offset of a specific spring
	SetSpringVerticalShift, // set the spring vertical shift of a specific spring

	SetSpringBreakingForce, // set the spring breaking force of a specific spring
	SetSpringBreakingLength,   // set the spring breaking length of a specific spring
	SetSpringDamageThreshold,  // set the spring damage threshold of a specific spring
	SetSpringWorkConst,        // set the spring work constant of a specific spring

	SetImpulseDamageThreshold, // set the impulse damage threshold of the cell manager
	SetImpulseDamageMultiplier,    // set the impulse damage multiplier of the cell manager
	SetMaxImpulseDamage,          // set the maximum impulse damage of the cell manager

	SetCellMatterLifetime,        // set the lifetime of cell matter

	// FOOD MANAGER EVENT  ──────────────────────────────────────────────────
	SetFoodToggles,
	ClearAllFood, // clears all food from the simulation

	SetRandomSpawnsPerFrame,
	SetRandomSpawnChance,
	SetMitosisConstant,

	SetPheromoneUpdatingSteps,
	SetPheromoneDecayRate,
	SetPheromoneDiffusionRate,
	SetPheromoneDepositAmount,
	SetPheromoneMaxPheromone,

	SetPheromoneUpdateFrequency,
	SetPheromoneRenderFrequency,

	SetMaxNutrients,
	SetFoodNutrientsDevelopmentTime,
	SetFoodFriction,
	SetFoodVibrationStrength,
	SetFoodReproductiveCooldown,
	SetFoodDeathAge,
	SetFoodReproductiveThreshold,
};

// Parameters for the MutateProtozoa command
struct MutateParams
{
	float mut_rate = 0.3f;
	float mut_range = 0.3f;
};

// Only one of these is meaningful per command — think of it like a union but without the hassle
struct SimCommand
{
	CommandSection section;
	CommandType  type; // identifies which of the following fields to read

	SimulationToggles sim_toggles;
	CellManagerToggles cell_toggles;
	WorldToggles world_toggles;
	FoodToggles  food_toggles;

	MutateParams mutate{};

	float        float_val = 0;
	int          int_val = 0;
	int          cell_spring_idx = 0;
	bool         bool_val = false;

	sf::Color    color_val_a{};
	sf::Color    color_val_b{};
};


struct ImGuiContext
{
	SimulationToggles& sim_toggles;
	CellManagerToggles& cell_toggles;
	WorldToggles& world_toggles;
	FoodToggles& food_toggles;

	std::mutex& cmd_mutex;
	std::queue<SimCommand>& commands;

	// Helper so tabs don't need to write the lock_guard boilerplate
	void push(SimCommand cmd)
	{
		std::lock_guard<std::mutex> lock(cmd_mutex);
		commands.push(std::move(cmd));
	}
};