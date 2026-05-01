#pragma once
#include "../world/world_state.h"

#include <mutex>
#include <queue>

enum class CommandType
{
    // ── Toggle state (whole WorldToggles struct carried in payload) ───────
    SetToggles,

    // ── One-shot actions ──────────────────────────────────────────────────
    ResetSimulation,        // open extinction popup confirmed
    ClearAllFood,
    ClearAllProtozoa,
    NavToProtozoa,

    // ── Spawn ─────────────────────────────────────────────────────────────
    SpawnRandom,            // spawn N random organisms
    SpawnFromFile,          // stub

    // ── Grid ──────────────────────────────────────────────────────────────
    SetCellGridResolution,      // apply new cell grid resolution
    SetFoodGridResolution,

    // ── World values (not in WorldToggles) ───────────────────────────────
    SetSimulationSpeed,     // the speed multiplier slider
    SetWorldRadius,         // world resize stub

	// ── Cell-specific (payload identifies cell and new values) ─────────────
    SetRadius,
	SetAmplitude,
	SetFrequency,
	SetOffset,
    SetVerticalShift,

    // Protozoa
	MutateProtozoa,         // mutate a specific protozoa (payload identifies protozoa and mutation parameters)
	AddCell,
	RemoveCell,           
	AddSpring,	
	RemoveSpring,
	InjectProtozoa,        // inject a protozoa with specific genome parameters
    MakeImmortal,
	ForceReproduce,
	KillProtozoa,
	CloneProtozoa,         // spawn a mutated clone of the selected protozoa

    // Springs
    SetSpringConst,
    SetDampingConst,
    SetSpringAmplitude,
    SetSpringFrequency,
    SetSpringOffset,
    SetSpringVerticalShift,

};

struct SpawnParams
{
    int   count = 10;
    bool  mutate = false;
    float mut_rate = 0.3f;
    float mut_range = 0.3f;
};

struct MutateParams
{
    float mut_rate = 0.3f;
    float mut_range = 0.3f;
};


// Only one of these is meaningful per command — think of it like a union but without the hassle
struct SimCommand
{
	CommandType  type; // identifies which of the following fields to read
    

    WorldToggles toggles{};     // for SetToggles
    SpawnParams  spawn{};       // for SpawnRandom
    MutateParams mutate{};

    float        float_val = 0;
    int          int_val = 0;
    int          cell_spring_idx = 0;
    bool         bool_val = false;
};


struct ImGuiContext
{
    WorldToggles& toggles;   // write toggles here freely
    std::mutex& cmd_mutex;
    std::queue<SimCommand>& commands;

    // Helper so tabs don't need to write the lock_guard boilerplate
    void push(SimCommand cmd)
    {
        std::lock_guard<std::mutex> lock(cmd_mutex);
        commands.push(std::move(cmd));
    }

    void test()
    {
        // adding a random simcommand to the imgui context to test it
		SimCommand cmd;
		cmd.type = CommandType::SpawnRandom;
		cmd.spawn.count = 5;
		cmd.spawn.mutate = true;
		cmd.spawn.mut_rate = 0.5f;
		cmd.spawn.mut_range = 0.5f;
		push(cmd);
    }
};