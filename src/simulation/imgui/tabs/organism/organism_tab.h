#pragma once
#include "../i_tab.h"
#include "energy/energy_history.h"
#include <cstdint>
#include <entities/body/body.h>
#include <entities/cell/cell.h>
#include <entities/spring/spring.h>
#include <imgui.h>
#include <managers/cell_manager/organism_tracker/organism_tracker.h>
#include <SFML/System/Vector2.hpp>
#include <simulation/context/sim_command.h>
#include <simulation/context/sim_snapshot.h>
#include <limits>
#include <vector>


// ─────────────────────────────────────────────────────────────────────────────
//  Energy tab: palette
// ─────────────────────────────────────────────────────────────────────────────
inline static constexpr ImVec4 k_en_col_energy_line = { 0.35f, 0.85f, 0.40f, 1.f };
inline static constexpr ImVec4 k_en_col_nutrients = { 0.20f, 0.60f, 1.00f, 1.f };
inline static constexpr ImVec4 k_en_col_integrity = { 0.85f, 0.80f, 0.25f, 1.f };
inline static constexpr ImVec4 k_en_col_cell_band = { 0.70f, 0.72f, 0.85f, 1.f };

inline static constexpr ImVec4 k_en_col_card_bg = { 0.13f, 0.14f, 0.17f, 1.f };
inline static constexpr ImVec4 k_en_col_card_border = { 0.28f, 0.30f, 0.37f, 1.f };
inline static constexpr ImVec4 k_en_col_cell_bg = { 0.18f, 0.19f, 0.23f, 1.f };
inline static constexpr ImVec4 k_en_col_dim = { 0.55f, 0.58f, 0.65f, 1.f };

inline static constexpr ImVec4 k_en_col_good = { 0.30f, 0.85f, 0.35f, 1.f };
inline static constexpr ImVec4 k_en_col_bad = { 0.90f, 0.30f, 0.30f, 1.f };
inline static constexpr ImVec4 k_en_col_warn = { 1.00f, 0.75f, 0.20f, 1.f };

// Cell state colours (left accent stripe + badge)
inline static constexpr ImVec4 k_en_state_ok = { 0.45f, 0.47f, 0.52f, 1.f };
inline static constexpr ImVec4 k_en_state_starving = { 0.90f, 0.25f, 0.25f, 1.f };
inline static constexpr ImVec4 k_en_state_damaged = { 0.95f, 0.55f, 0.20f, 1.f };
inline static constexpr ImVec4 k_en_state_ready = { 0.30f, 0.85f, 0.35f, 1.f };
inline static constexpr ImVec4 k_en_state_digesting = { 0.65f, 0.50f, 0.95f, 1.f };
inline static constexpr ImVec4 k_en_state_immortal = { 0.35f, 0.85f, 0.85f, 1.f };

// ─────────────────────────────────────────────────────────────────────────────
//  Energy tab: layout + behaviour constants
//  Layout is a single row of full-height cards. Fixed cards keep their width;
//  History and Cells share whatever is left (55 / 45). The Constants card is
//  dropped when the panel is too narrow to keep History and Cells usable.
// ─────────────────────────────────────────────────────────────────────────────
static constexpr float k_en_gauges_w = 150.f;
static constexpr float k_en_budget_w = 200.f;
static constexpr float k_en_feed_w = 150.f;
static constexpr float k_en_consts_w = 210.f;
static constexpr float k_en_history_min_w = 340.f;
static constexpr float k_en_cells_min_w = 260.f;
static constexpr float k_en_history_share = 0.55f;

static constexpr float k_cell_card_w = 112.f;   // widened so "Cell 12" + the state badge fit
static constexpr float k_cell_card_h = 42.f;
static constexpr float k_cell_card_gap = 4.f;
static constexpr float k_strip_block = 18.f;
static constexpr int   k_strip_auto_threshold = 24;   // "Auto" view switches to the heat-strip above this many cells

static constexpr float k_starve_frac = 0.15f;   // energy below this fraction of max  -> Starving
static constexpr float k_damaged_frac = 0.30f;   // integrity below this fraction      -> Damaged

static constexpr int   k_energy_windows[] = { 300, 1000, 3000 };   // history window, in ticks
static constexpr int   k_trend_window = 120;     // samples used for slope / trend arrows / ETAs
static constexpr float k_trend_eps = 0.003f;  // |change per 100 ticks| (fraction of max) below which a trend reads as flat

// Is the InjectProtozoa amount added to the organism total (false) or to every cell (true)?
// Only affects the "after inject" preview in the Feed card; verify against the handler.
static constexpr bool  k_inject_is_per_cell = false;

// Sentinel for "nothing selected" in the ID-based selection keys.
static constexpr uint32_t k_no_key = 0xFFFFFFFFu;

// Instantaneous organism energy numbers, shared by all Energy-tab cards.
// Fractions are relative to the organism maximum (0..1).
struct EnergyStats
{
	int   n = 0;
	float frac[3] = {};     // energy, nutrients, integrity
	float thr[3] = {};     // reproduction threshold as a fraction of organism max
	float slope[3] = {};     // measured change per tick (fraction), from EnergyHistory
	float total_integrity = 0.f, max_integrity = 0.f;
	float net_dE = 0.f, net_dI = 0.f;                    // sum of per-cell delta_energy / delta_integrity
	float cell_min = 0.f, cell_mean = 0.f, cell_max = 0.f; // per-cell energy fractions
};

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
	// ── Selection ─────────────────────────────────────────────────────────
	// Selection is stored by ID (the *_key_ fields). The *_idx_ fields are tracker
	// indices that rebuild_ordering() re-resolves from the keys at the start of every frame.
	int      m_last_id_ = -1;                // energy-history organism tracking (record_energy_history)
	int      m_sel_org_id_ = -1;             // organism the keys below belong to
	int      m_sel_cell_idx_ = 0;
	int      m_sel_spring_idx_ = 0;
	bool     m_sel_is_spring_ = false;
	uint32_t m_sel_cell_key_ = k_no_key;
	uint32_t m_sel_spring_key_ = k_no_key;
	uint32_t m_pushed_highlight_ = k_no_key; // last cell ID sent to the world for the white circle

	// Sorted-by-ID views of the tracker, rebuilt every frame.
	//   m_cell_order_[k] = tracker index of the k-th cell (k is the displayed "Cell k")
	//   m_cell_rank_[i]  = relative number of the cell at tracker index i
	std::vector<int>      m_cell_order_, m_cell_rank_;
	std::vector<int>      m_spring_order_, m_spring_rank_;
	std::vector<uint32_t> m_sorted_cell_keys_;   // cell IDs in sorted order, for lookup by absolute ID

	int  m_wave_cycles_ = 8;   // number of full periods shown in the sinwave graph

	// ── Feed state (owned here, shared by Energy tab) ─────────────────────
	int   m_feed_mode_ = 0;     // 0 = energy,  1 = nutrients
	int   cell_selection_mode = 0;
	float m_feed_amount_ = 50.f;

	// ── Energy tab state ──────────────────────────────────────────────────
	EnergyHistory m_energy_hist_;
	long long     m_hist_last_clock_ = -1;   // last cells[0].internal_clock_ seen
	float         m_hist_tick_ = 0.f;        // monotonic tick counter built from clock deltas
	int   m_energy_window_ = 1;              // index into k_energy_windows
	bool  m_show_series_[4] = { true, true, true, true };   // NRG, NUT, INT, per-cell band
	int   m_cell_sort_ = 0;                  // 0 index, 1 energy (low), 2 integrity (low), 3 food (high)
	int   m_cell_view_ = 0;                  // 0 auto, 1 cards, 2 heat-strip
	int   m_strip_metric_ = 0;               // 0 energy, 1 nutrients, 2 integrity

	// ── ID helpers ────────────────────────────────────────────────────────
	static uint32_t cell_key(const Cell& c) { return c.id_; }   // switch to body_id_ if springs reference that
	static uint32_t spring_key(const Tendon& s) { return static_cast<uint32_t>(s.id_); }
	void rebuild_ordering(const OrganismTracker& t);
	void sync_selection(ImGuiContext& ctx, const OrganismTracker& t);
	void push_highlight(ImGuiContext& ctx, uint32_t key);
	int  relative_cell_index(uint32_t abs_id) const;   // -1 if the cell is not in this organism

	// ── Drawing ───────────────────────────────────────────────────────────
	void draw_overview(const SimSnapshot& snap, ImGuiContext& ctx, const OrganismTracker& protozoa);
	static void draw_no_selection();
	void draw_cells_springs_tab(const SimSnapshot& snap, ImGuiContext& ctx, const OrganismTracker& protozoa);
	void draw_mutation_controls(const SimSnapshot& snap, ImGuiContext& ctx, const OrganismTracker& protozoa);

	void draw_wave_panel(ImGuiContext& ctx, const char* child_id, const char* description,
		double now, int idx, const char* value_label, float actual_value, ImVec4 col,
		const WaveParam& amplitude, const WaveParam& frequency,
		const WaveParam& offset, const WaveParam& vertical_shift);

	void draw_cell_detail(ImGuiContext& ctx, const Cell& c, const Body& body);
	void draw_spring_detail(ImGuiContext& ctx, const OrganismTracker& p, const Tendon& s, int spring_rel);
	void draw_energy_tab(ImGuiContext& ctx, const SimSnapshot& snap);
	void record_energy_history(const SimSnapshot& snap);   // called every frame from draw(), whatever tab is open
	void draw_energy_gauges(const EnergyStats& st, const OrganismTracker& t);
	void draw_energy_history(const EnergyStats& st, float w);
	void draw_energy_budget(const EnergyStats& st, const OrganismTracker& t, bool constants_in_tooltip);
	void draw_energy_cells(const OrganismTracker& t, float w);
	void draw_energy_feed(ImGuiContext& ctx, const EnergyStats& st, const OrganismTracker& t);
	static void draw_energy_constants();
	void Brain();

	void draw_cell_detail_cell_tab(const Cell& c, int period, float wave_min, float wave_max, float current_friction);
	void draw_cell_detail_body_tab(ImGuiContext& ctx, const Cell& c, const Body& body);
};