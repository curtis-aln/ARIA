// Energy tab: Budget card (flows, trend, ETAs) and the shared Constants table.
#include "energy_ui_common.h"
#include <cmath>
#include <cstdio>

using namespace en_ui;

namespace
{
	// Index of the energy gauge in EnergyStats' per-gauge arrays (same order as EnergyHistory::Series)
	constexpr int k_energy_slot = static_cast<int>(EnergyHistory::Energy);

	// Values closer to zero than this are treated as "no change"
	constexpr float k_zero_eps = 1e-6f;

	// ETA estimates are coloured as urgent below this many frames
	constexpr float k_starve_urgent_frames = 1000.f;

	// An ETA string plus the colour it should be drawn in
	struct EtaText
	{
		char   text[24];
		ImVec4 color;
	};

	// ── Formatting ───────────────────────────────────────────────────────────
	void format_frames(char* out, const size_t size, const float frames)
	{
		if (!std::isfinite(frames) || frames < 0.f) std::snprintf(out, size, "--");
		else if (frames > 99999.f)                  std::snprintf(out, size, ">99999 fr");
		else                                        std::snprintf(out, size, "%.0f fr", frames);
	}

	// Green when rising, red when falling, dim when flat
	ImVec4 signed_value_color(const float value)
	{
		if (value > k_zero_eps)  return k_en_col_good;
		if (value < -k_zero_eps) return k_en_col_bad;
		return k_en_col_dim;
	}

	// ── Layout helpers ───────────────────────────────────────────────────────
	// Moves the cursor right so the next text item ends flush with the right edge of the available space
	void align_cursor_right(const char* text)
	{
		const float padding = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(text).x;
		if (padding > 0.f)
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + padding);
	}

	// Two-column table row: label on the left, coloured value right-aligned
	void table_row_colored(const char* label, const char* value, const ImVec4& color)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextUnformatted(label);
		ImGui::TableSetColumnIndex(1);
		align_cursor_right(value);
		ImGui::TextColored(color, "%s", value);
	}

	// Two-column table row: label on the left, plain number right-aligned
	void table_row_number(const char* label, const float value, const int decimals)
	{
		char text[24];
		std::snprintf(text, sizeof text, "%.*f", decimals, value);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextUnformatted(label);
		ImGui::TableSetColumnIndex(1);
		align_cursor_right(text);
		ImGui::TextUnformatted(text);
	}

	void table_section_header(const char* title)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextDisabled("%s", title);
	}

	// "Label ........ value" on one line, value coloured and right-aligned
	void draw_label_and_right_value(const char* label, const EtaText& value)
	{
		ImGui::TextUnformatted(label);
		ImGui::SameLine();
		align_cursor_right(value.text);
		ImGui::TextColored(value.color, "%s", value.text);
	}

	// ── Projections (from the measured energy trend) ─────────────────────────
	// How long until the organism's energy runs out, if the current trend continues
	EtaText compute_starve_eta(const EnergyStats& stats)
	{
		const float fill = stats.fill_fraction[k_energy_slot];
		const float slope = stats.slope_per_tick[k_energy_slot];

		EtaText eta{ "--", k_en_col_dim };

		if (fill <= 0.f)
		{
			std::snprintf(eta.text, sizeof eta.text, "empty");
			eta.color = k_en_col_bad;
		}
		else if (slope * 100.f < -k_trend_eps)
		{
			const float frames_left = fill / -slope;
			format_frames(eta.text, sizeof eta.text, frames_left);
			eta.color = frames_left < k_starve_urgent_frames ? k_en_col_bad : k_en_col_warn;
		}
		return eta;
	}

	// How long until the organism's energy reaches the reproduction threshold, if the trend continues
	EtaText compute_repro_eta(const EnergyStats& stats)
	{
		const float fill = stats.fill_fraction[k_energy_slot];
		const float threshold = stats.repro_threshold_fraction[k_energy_slot];
		const float slope = stats.slope_per_tick[k_energy_slot];

		EtaText eta{ "--", k_en_col_dim };

		if (threshold > 0.f && fill >= threshold)
		{
			std::snprintf(eta.text, sizeof eta.text, "now");
			eta.color = k_en_col_good;
		}
		else if (threshold > 0.f && slope * 100.f > k_trend_eps)
		{
			format_frames(eta.text, sizeof eta.text, (threshold - fill) / slope);
			eta.color = k_en_col_good;
		}
		return eta;
	}

	// ── Flows table ──────────────────────────────────────────────────────────
	// Instantaneous per-frame flows plus the measured energy trend
	void draw_budget_flows(const EnergyStats& stats, const OrganismTracker& tracker)
	{
		if (!ImGui::BeginTable("##budget", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
			return;

		ImGui::TableSetupColumn("k", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("v", ImGuiTableColumnFlags_WidthFixed, 74.f);

		char text[32];

		std::snprintf(text, sizeof text, "%+.3f", stats.net_energy_delta);
		table_row_colored("Net dE", text, signed_value_color(stats.net_energy_delta));

		std::snprintf(text, sizeof text, "%.3f", -tracker.spring_total_work_done);
		table_row_colored("Spring drain", text,
			tracker.spring_total_work_done > k_zero_eps ? k_en_col_bad : k_en_col_dim);

		std::snprintf(text, sizeof text, "%+.3f", stats.net_integrity_delta);
		table_row_colored("dIntegrity", text, signed_value_color(stats.net_integrity_delta));

		// Trend is stored as fill fraction per tick; convert to absolute energy per 100 frames
		const float energy_trend_per_100 = stats.slope_per_tick[k_energy_slot] * 100.f * tracker.max_energy;
		std::snprintf(text, sizeof text, "%+.2f", energy_trend_per_100);
		table_row_colored("dE / 100 fr", text, signed_value_color(energy_trend_per_100));

		ImGui::EndTable();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Budget card: instantaneous flows + measured trend + ETAs
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_energy_budget(const EnergyStats& stats, const OrganismTracker& tracker,
	const bool constants_in_tooltip)
{
	if (begin_card("EN_budget", k_en_budget_w))
	{
		ImGui::TextDisabled("Budget  (per frame)");

		// When the Constants card is hidden for lack of width, offer the table as a hover tooltip instead
		if (constants_in_tooltip)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("(?)");
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::TextDisabled("Constants");
				ImGui::Separator();
				draw_energy_constants();
				ImGui::EndTooltip();
			}
		}
		ImGui::Separator();

		draw_budget_flows(stats, tracker);

		ImGui::Spacing();
		ImGui::TextDisabled("Projection  (from trend)");
		ImGui::Separator();

		draw_label_and_right_value("Starve in", compute_starve_eta(stats));
		draw_label_and_right_value("Repro in", compute_repro_eta(stats));

		ImGui::Spacing();
		ImGui::TextDisabled("Energy %.0f%%  /  repro at %.0f%%",
			stats.fill_fraction[k_energy_slot] * 100.f,
			stats.repro_threshold_fraction[k_energy_slot] * 100.f);
	}
	ImGui::EndChild();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Constants table (used by the Constants card and by the Budget "(?)" tooltip)
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_energy_constants()
{
	if (!ImGui::BeginTable("##consts", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
		return;

	ImGui::TableSetupColumn("k", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("v", ImGuiTableColumnFlags_WidthFixed, 64.f);

	table_section_header("Conversion (per frame, per cell)");
	table_row_number("Nutrients -> Energy", CellSettings::nutrients_conversion_rate, 5);
	table_row_number("Energy -> Integrity", CellSettings::integrity_conversion_rate, 5);
	table_row_number("Spring work cost", SpringSettings::spring_work_const, 5);

	table_section_header("Limits (per cell)");
	table_row_number("Initial energy", CellSettings::initial_energy, 1);

	ImGui::EndTable();
}