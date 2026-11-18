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
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
//  Genes tab: view state (everything else is read from the snapshot each frame)
// ─────────────────────────────────────────────────────────────────────────────
enum class GnMetric : uint8_t { Value, Gene, Sigma };

struct GenesTabState
{
	GnMetric metric = GnMetric::Value;
	bool     sort_by_spread = false;
	bool     hide_converged = false;
	bool     show_flags = true;
	char     filter[32] = {};
	uint32_t closed_groups = 0;       // bit per GeneGroup, 1 = folded
	bool     initialised = false;
	bool     sel_is_tendon = false;   // which genome kind the inspector shows
	int      sel_gene = 0;
	int      wave_window = 240;       // frames shown in the wave overlay (slider range 60..600)
};


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
	int   cell_count = 0;
	float fill_fraction[3] = {};     // energy, nutrients, integrity
	float repro_threshold_fraction[3] = {};     // reproduction threshold as a fraction of organism max
	float slope_per_tick[3] = {};     // measured change per tick (fraction), from EnergyHistory
	float total_integrity = 0.f, max_integrity = 0.f;
	float net_energy_delta = 0.f, net_integrity_delta = 0.f;                    // sum of per-cell delta_energy / delta_integrity
	float cell_energy_min = 0.f, cell_energy_mean = 0.f, cell_energy_max = 0.f; // per-cell energy fractions
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
inline static constexpr ImVec4 integrity_bar_col = { 0.80f, 0.50f, 0.00f, 1.f }; // orange

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

inline constexpr float k_speed_ref = 0.5f;   // speed at which the compass arrow reaches ~76% of its radius; tune to your sim

inline ImU32 col32(const ImVec4& c, const float a) { return ImGui::ColorConvertFloat4ToU32({ c.x, c.y, c.z, a }); }

inline bool hover_rect(const ImVec2 a, const ImVec2 b) { return ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(a, b); }

// Rounded status chip, right-aligned to `right_mid`.
inline void draw_pill(ImDrawList* dl, const ImVec2 right_mid, const char* text, const ImVec4& col)
{
	const ImVec2 ts = ImGui::CalcTextSize(text);
	constexpr float padx = 7.f;
	const float h = ts.y + 4.f;
	const ImVec2 p0 = { right_mid.x - ts.x - padx * 2.f, right_mid.y - h * 0.5f };
	const ImVec2 p1 = { right_mid.x, right_mid.y + h * 0.5f };
	dl->AddRectFilled(p0, p1, col32(col, 0.22f), h * 0.5f);
	dl->AddRect(p0, p1, col32(col, 0.80f), h * 0.5f, 0, 1.f);
	dl->AddText({ p0.x + padx, p0.y + 2.f }, col32(col, 1.f), text);
}

// Dark tile: big number, dim label underneath, inset accent bar on the left.
inline void draw_tile(ImDrawList* dl, const ImVec2 p0, const ImVec2 size, const char* value, const char* label, const ImVec4& accent)
{
	const ImVec2 p1 = { p0.x + size.x, p0.y + size.y };
	dl->AddRectFilled(p0, p1, IM_COL32(26, 28, 34, 255), 5.f);
	dl->AddRectFilled({ p0.x + 4.f, p0.y + 7.f }, { p0.x + 7.f, p1.y - 7.f }, col32(accent, 1.f), 1.5f);
	ImFont* font = ImGui::GetFont();
	const float fs = ImGui::GetFontSize();
	dl->AddText(font, fs * 1.35f, { p0.x + 13.f, p0.y + 3.f }, IM_COL32(232, 236, 246, 255), value);
	dl->AddText({ p0.x + 13.f, p0.y + 3.f + fs * 1.35f }, IM_COL32(135, 141, 158, 255), label);
}

// Velocity compass: arrow points where the organism is heading (screen space, +y down), length = speed.
inline void draw_compass(ImDrawList* dl, const ImVec2 c, const float r, const float vx, const float vy, const float speed)
{
	dl->AddCircleFilled(c, r, IM_COL32(26, 28, 34, 255), 40);
	dl->AddCircle(c, r * 0.5f, IM_COL32(255, 255, 255, 16), 32, 1.f);
	dl->AddLine({ c.x - r, c.y }, { c.x + r, c.y }, IM_COL32(255, 255, 255, 16));
	dl->AddLine({ c.x, c.y - r }, { c.x, c.y + r }, IM_COL32(255, 255, 255, 16));
	dl->AddCircle(c, r, IM_COL32(255, 255, 255, 45), 40, 1.f);

	const float mag = std::sqrt(vx * vx + vy * vy);
	const float t = std::tanh(speed / k_speed_ref);
	const float len = (r - 5.f) * t;
	if (mag > 1e-6f && len > 2.f)
	{
		const ImVec2 d = { vx / mag, vy / mag };
		const ImVec2 n = { -d.y, d.x };
		const ImU32 col = IM_COL32(static_cast<int>(100 + 30 * t), static_cast<int>(150 + 85 * t), static_cast<int>(210 + 45 * t), 255);
		const float head = std::min(7.f, len * 0.7f);
		const ImVec2 tip = { c.x + d.x * len, c.y + d.y * len };
		const ImVec2 base = { tip.x - d.x * head, tip.y - d.y * head };
		dl->AddLine(c, base, col, 2.5f);
		dl->AddTriangleFilled(tip, { base.x + n.x * head * 0.55f, base.y + n.y * head * 0.55f },
			{ base.x - n.x * head * 0.55f, base.y - n.y * head * 0.55f }, col);
	}
	dl->AddCircleFilled(c, 2.5f, IM_COL32(220, 228, 245, 255), 12);
}

// Rounded stat bar: name left, value right, gloss strip, red pulsing outline below `pulse_below`.
inline void stat_bar(const char* name, const float value, const float max_value, const ImVec4& col, const float pulse_below = 0.f)
{
	constexpr float h = 18.f;
	const float w = ImGui::GetContentRegionAvail().x;
	const ImVec2 p0 = ImGui::GetCursorScreenPos(), p1 = { p0.x + w, p0.y + h };
	ImGui::Dummy({ w, h });

	const float f = max_value > 0.f ? std::clamp(value / max_value, 0.f, 1.f) : 0.f;
	ImDrawList* dl = ImGui::GetWindowDrawList();

	dl->AddRectFilled(p0, p1, IM_COL32(26, 28, 34, 255), 5.f);
	if (f > 0.f)
	{
		const float fx = p0.x + w * f;
		dl->AddRectFilled(p0, { fx, p1.y }, col32(col, 0.90f), 5.f);
		dl->AddRectFilled({ p0.x + 2.f, p0.y + 2.f }, { std::max(p0.x + 2.f, fx - 2.f), p0.y + h * 0.42f },
			IM_COL32(255, 255, 255, 30), 3.f);   // gloss
	}

	if (f < pulse_below)
	{
		const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 6.f);
		dl->AddRect(p0, p1, IM_COL32(255, 70, 70, static_cast<int>(70 + 150 * pulse)), 5.f, 0, 1.5f);
	}
	else
		dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 22), 5.f);

	char val[32];
	snprintf(val, sizeof val, "%.0f / %.0f", value, max_value);
	const float ty = p0.y + (h - ImGui::GetTextLineHeight()) * 0.5f;
	const auto shadowed = [&](const ImVec2 p, const char* s)
		{
			dl->AddText({ p.x + 1.f, p.y + 1.f }, IM_COL32(0, 0, 0, 170), s);
			dl->AddText(p, IM_COL32(240, 243, 250, 255), s);
		};
	shadowed({ p0.x + 7.f, ty }, name);
	shadowed({ p1.x - 6.f - ImGui::CalcTextSize(val).x, ty }, val);

	if (hover_rect(p0, p1))
		ImGui::SetTooltip("%s  %.1f%%", name, f * 100.f);
}

class OrganismTab : public ITab
{
public:
	const char* label() const override { return "Organism"; }
	void        draw(const SimSnapshot& snap, ImGuiContext& ctx)   override;

private:
	// ── Selection ─────────────────────────────────────────────────────────
	// Selection is stored by ID (the *_key_ fields). The *_idx_ fields are tracker
	// indices that rebuild_ordering() re-resolves from the keys at the start of every frame.
	int      m_history_organism_id_ = -1;                // energy-history organism tracking (record_energy_history)
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

	// -- Genome state
	GenesTabState m_genes_;

	// ── Energy tab state ──────────────────────────────────────────────────
	EnergyHistory m_energy_history_;
	long long     m_history_last_clock_ = -1;   // last cells[0].internal_clock_ seen
	float         m_history_tick_ = 0.f;        // monotonic tick counter built from clock deltas
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

	void draw_genes_tab(const SimSnapshot& snap, ImGuiContext& ctx, const OrganismTracker& protozoa);

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