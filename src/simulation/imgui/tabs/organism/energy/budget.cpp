// Energy tab — Budget card (flows, trend, ETAs) and the shared Constants table.
#include "energy_ui_common.h"
#include <cmath>
#include <cstdio>

using namespace en_ui;

namespace
{
	void fmt_frames(char* out, size_t n, float frames)
	{
		if (!std::isfinite(frames) || frames < 0.f) std::snprintf(out, n, "--");
		else if (frames > 99999.f)                  std::snprintf(out, n, ">99999 fr");
		else                                        std::snprintf(out, n, "%.0f fr", frames);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Budget — instantaneous flows + measured trend + ETAs
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_energy_budget(const EnergyStats& st, const OrganismTracker& t, bool constants_in_tooltip)
{
	if (begin_card("EN_budget", k_en_budget_w))
	{
		ImGui::TextDisabled("Budget  (per frame)");
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

		auto signed_col = [](float v) { return v > 1e-6f ? k_en_col_good : (v < -1e-6f ? k_en_col_bad : k_en_col_dim); };

		if (ImGui::BeginTable("##budget", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn("k", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("v", ImGuiTableColumnFlags_WidthFixed, 74.f);

			auto row = [](const char* label, const char* value, const ImVec4& col)
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(label);
					ImGui::TableSetColumnIndex(1);
					const float pad = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(value).x;
					if (pad > 0.f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pad);
					ImGui::TextColored(col, "%s", value);
				};

			char buf[32];
			std::snprintf(buf, sizeof buf, "%+.3f", st.net_dE);
			row("Net dE", buf, signed_col(st.net_dE));

			std::snprintf(buf, sizeof buf, "%.3f", -t.spring_total_work_done);
			row("Spring drain", buf, t.spring_total_work_done > 1e-6f ? k_en_col_bad : k_en_col_dim);

			std::snprintf(buf, sizeof buf, "%+.3f", st.net_dI);
			row("dIntegrity", buf, signed_col(st.net_dI));

			const float e_trend = st.slope[0] * 100.f * t.max_energy;
			std::snprintf(buf, sizeof buf, "%+.2f", e_trend);
			row("dE / 100 fr", buf, signed_col(e_trend));

			ImGui::EndTable();
		}

		ImGui::Spacing();
		ImGui::TextDisabled("Projection  (from trend)");
		ImGui::Separator();

		const float per100 = st.slope[0] * 100.f;
		char starve[24], repro[24];
		ImVec4 starve_col = k_en_col_dim, repro_col = k_en_col_dim;

		if (st.frac[0] <= 0.f)
		{
			std::snprintf(starve, sizeof starve, "empty");
			starve_col = k_en_col_bad;
		}
		else if (per100 < -k_trend_eps)
		{
			const float fr = st.frac[0] / -st.slope[0];
			fmt_frames(starve, sizeof starve, fr);
			starve_col = fr < 1000.f ? k_en_col_bad : k_en_col_warn;
		}
		else
			std::snprintf(starve, sizeof starve, "--");

		if (st.thr[0] > 0.f && st.frac[0] >= st.thr[0])
		{
			std::snprintf(repro, sizeof repro, "now");
			repro_col = k_en_col_good;
		}
		else if (st.thr[0] > 0.f && per100 > k_trend_eps)
		{
			fmt_frames(repro, sizeof repro, (st.thr[0] - st.frac[0]) / st.slope[0]);
			repro_col = k_en_col_good;
		}
		else
			std::snprintf(repro, sizeof repro, "--");

		ImGui::TextUnformatted("Starve in");
		ImGui::SameLine();
		{
			const float pad = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(starve).x;
			if (pad > 0.f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pad);
			ImGui::TextColored(starve_col, "%s", starve);
		}
		ImGui::TextUnformatted("Repro in");
		ImGui::SameLine();
		{
			const float pad = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(repro).x;
			if (pad > 0.f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pad);
			ImGui::TextColored(repro_col, "%s", repro);
		}

		ImGui::Spacing();
		ImGui::TextDisabled("Energy %.0f%%  /  repro at %.0f%%", st.frac[0] * 100.f, st.thr[0] * 100.f);
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

	auto section = [](const char* s)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextDisabled("%s", s);
		};
	auto row = [](const char* k, float v, int decimals)
		{
			char buf[24];
			std::snprintf(buf, sizeof buf, "%.*f", decimals, v);
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(k);
			ImGui::TableSetColumnIndex(1);
			const float pad = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(buf).x;
			if (pad > 0.f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pad);
			ImGui::TextUnformatted(buf);
		};

	section("Conversion (per frame, per cell)");
	row("Nutrients -> Energy", CellSettings::nutrients_conversion_rate, 5);
	row("Energy -> Integrity", CellSettings::integrity_conversion_rate, 5);
	row("Spring work cost", SpringSettings::spring_work_const, 5);

	section("Limits (per cell)");
	row("Initial energy", CellSettings::initial_energy, 1);
	row("Max energy", CellSettings::max_energy, 1);
	row("Max integrity", CellSettings::max_integrity, 1);
	row("Max nutrients", CellSettings::max_nutrients, 1);

	ImGui::EndTable();
}