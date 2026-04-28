#pragma once
#include "../world/world_state.h"

#include <mutex>
#include <queue>

// Discriminant tag for SimCommand. Each variant maps to exactly one
// field (or sub-struct) inside SimCommand's payload — see field comments there.
// Stubs (SpawnFromFile, SetWorldRadius, ResetSimulation) are declared
// but not yet handled in simulation.cpp's command dispatch switch.
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


// ─────────────────────────────────────────────────────────────────────────────
// Render → sim message. Written by the ImGui/render thread, consumed at the
// top of every sim tick before physics runs.
//
// Layout is a manual tagged union: `type` selects which payload field is
// meaningful; all others are ignored. One command = one mutation intent.
// Queued in Simulation::m_commands under m_cmd_mutex.
// ─────────────────────────────────────────────────────────────────────────────
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

 
// ─────────────────────────────────────────────────────────────────────────────
// Per-frame UI execution context passed down into every tab's draw() call.
// Bundles the mutable toggles copy (tabs read/write this freely) and a
// reference to the command queue so tabs can fire sim mutations without
// knowing about the Simulation class.
// ─────────────────────────────────────────────────────────────────────────────
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