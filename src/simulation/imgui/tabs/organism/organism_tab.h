#pragma once
#include "../i_tab.h"
#include <entities/body.h>
#include <entities/cell/cell.h>
#include <entities/spring/spring.h>
#include <imgui.h>
#include <managers/cell_manager/organism_tracker.h>
#include <SFML/System/Vector2.hpp>
#include <simulation/context/sim_command.h>
#include <simulation/context/sim_snapshot.h>
#include <vector>


// Visual dimensions for the per-cell mini bars in the Energy tab.
static constexpr float k_mini_cell_box_width = 130.f;
static constexpr float k_mini_bar_height = 8.f;

// Placeholder upper bound for cell nutrients until a hard cap is added to
// ProtozoaSettings. Used for progress-bar scaling only — not enforced here.
static constexpr float k_summary_bar_height = 18.f;

inline static constexpr ImVec4 friction_wave_col = { 1.00f, 0.65f, 0.25f, 1.f };
inline static constexpr ImVec4 extension_wave_col = { 0.85f, 0.45f, 1.00f, 1.f };
inline static constexpr float  no_actual_value = std::numeric_limits<float>::quiet_NaN();


// ─────────────────────────────────────────────────────────────────────────────
//  Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

// Hard cap on sinwave buffer to prevent OOM when frequency is near zero.
static constexpr int k_max_wave_buf = 2048;

// Design constants
inline static constexpr ImVec4 nutrients_bar_col = { 0.35f, 0.75f, 0.35f, 1.f };
inline static constexpr ImVec4 selector_color = { 0.2f, 0.2f, 0.8f, 1.0f };
inline static constexpr ImVec2 spring_cell_box_size = { 300.f, -1.f };

// One genetic sine parameter: its current value, valid range, display format,
// and the SimCommand type that updates it.
struct WaveParam
{
	float       value;
	float       min;
	float       max;
	const char* fmt;
	CommandType type;
};

class OrganismTab : public ITab
{
public:
	const char* label() const override { return "Organism"; }
	void        draw(const SimSnapshot& snap, ImGuiContext& ctx)   override;

private:
	int  m_last_id_ = -1;
	int  m_sel_cell_idx_ = 0;
	int  m_sel_spring_idx_ = 0;
	bool m_sel_is_spring_ = false;
	int  m_wave_cycles_ = 8;   // number of full periods shown in the sinwave graph

	// ── Feed state (owned here, shared by Energy tab) ─────────────────────
	int   m_feed_mode_ = 0;     // 0 = energy,  1 = nutrients
	int   cell_selection_mode = 0;
	float m_feed_amount_ = 50.f;

	void draw_overview(const SimSnapshot& snap, ImGuiContext& ctx, const OrganismTracker& protozoa);
	static void draw_no_selection();
	void draw_cells_springs_tab(const SimSnapshot& snap, ImGuiContext& ctx, const OrganismTracker& protozoa);
	void draw_mutation_controls(const SimSnapshot& snap, ImGuiContext& ctx, const OrganismTracker& protozoa);
	
	void draw_wave_panel(ImGuiContext& ctx, const char* child_id, const char* description,
		double now, int idx, const char* value_label, float actual_value, ImVec4 col,
		const WaveParam& amplitude, const WaveParam& frequency,
		const WaveParam& offset, const WaveParam& vertical_shift);

	void draw_cell_detail(ImGuiContext& ctx, const Cell& c, const Body& body);
	void draw_spring_detail(ImGuiContext& ctx, const OrganismTracker& p, const Spring& s);
	void draw_energy_tab(ImGuiContext& ctx, const SimSnapshot& snap);
	void Brain();

	void draw_cell_detail_cell_tab(const Cell& c, int period, float wave_min, float wave_max, float current_friction);
	void draw_cell_detail_body_tab(ImGuiContext& ctx, const Cell& c, const Body& body);
};