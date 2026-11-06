// Energy tab — Feed card: mode toggle, amount + presets, "after inject" preview.
#include "energy_ui_common.h"
#include <cmath>
#include <cstdio>

using namespace en_ui;

namespace
{
	bool seg_button(const char* label, bool active, const ImVec4& active_col, const ImVec2& size)
	{
		const ImVec4 b = active ? active_col : ImVec4{ 0.20f, 0.21f, 0.25f, 1.f };
		ImGui::PushStyleColor(ImGuiCol_Button, b);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ b.x + 0.08f, b.y + 0.08f, b.z + 0.08f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ b.x + 0.14f, b.y + 0.14f, b.z + 0.14f, 1.f });
		const bool pressed = ImGui::Button(label, size);
		ImGui::PopStyleColor(3);
		return pressed;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Feed — mode toggle, amount + presets, "after inject" preview
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_energy_feed(ImGuiContext& ctx, const EnergyStats& st, const OrganismTracker& t)
{
	if (begin_card("EN_feed", k_en_feed_w))
	{
		ImGui::TextDisabled("Feed");
		ImGui::Separator();
		ImGui::Spacing();

		const float sp = ImGui::GetStyle().ItemSpacing.x;
		const float half = std::floor((ImGui::GetContentRegionAvail().x - sp) * 0.5f);

		if (seg_button("Energy##fmode", m_feed_mode_ == 0, { 0.20f, 0.55f, 0.25f, 1.f }, { half, 0.f }))
			m_feed_mode_ = 0;
		ImGui::SameLine();
		if (seg_button("Nutrients##fmode", m_feed_mode_ == 1, { 0.15f, 0.40f, 0.75f, 1.f }, { half, 0.f }))
			m_feed_mode_ = 1;

		ImGui::Spacing();
		ImGui::SetNextItemWidth(-1.f);
		ImGui::SliderFloat("##feed_amount", &m_feed_amount_, 1.f, 500.f, "Amount = %.0f");

		static constexpr float presets[4] = { 10.f, 50.f, 100.f, 500.f };
		for (int i = 0; i < 4; ++i)
		{
			if (i % 2) ImGui::SameLine();
			char lbl[16];
			std::snprintf(lbl, sizeof lbl, "%.0f##pre%d", presets[i], i);
			if (ImGui::Button(lbl, { half, 0.f }))
				m_feed_amount_ = presets[i];
		}

		// ── Preview ───────────────────────────────────────────────────────
		const bool energy_mode = m_feed_mode_ == 0;
		const float cur = energy_mode ? t.total_energy : t.total_nutrients;
		const float mx = energy_mode ? t.max_energy : t.max_nutrients;
		const float add = k_inject_is_per_cell ? m_feed_amount_ * static_cast<float>(st.n) : m_feed_amount_;
		const float after = cur + add;
		const bool over = after > mx;
		const ImVec4 mode_col = energy_mode ? k_en_col_energy_line : k_en_col_nutrients;

		ImGui::Spacing();
		ImGui::TextDisabled("After inject");
		{
			ImDrawList* dl = ImGui::GetWindowDrawList();
			const ImVec2 p = ImGui::GetCursorScreenPos();
			const float bw = ImGui::GetContentRegionAvail().x;
			const float f0 = safe_frac(cur, mx), f1 = safe_frac(after, mx);
			dl->AddRectFilled(p, { p.x + bw, p.y + 10.f }, u32({ 1.f, 1.f, 1.f, 0.08f }), 3.f);
			dl->AddRectFilled(p, { p.x + bw * f1, p.y + 10.f }, u32(over ? k_en_col_warn : mode_col, 0.45f), 3.f);
			dl->AddRectFilled(p, { p.x + bw * f0, p.y + 10.f }, u32(mode_col), 3.f);
			ImGui::Dummy({ bw, 10.f });
		}
		ImGui::TextColored(over ? k_en_col_warn : k_en_col_dim, "%.0f -> %.0f / %.0f", cur, after, mx);
		if (over)
			ImGui::SetItemTooltip("Exceeds the organism maximum");

		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.18f, 0.45f, 0.22f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.25f, 0.58f, 0.30f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.32f, 0.70f, 0.38f, 1.f });
		if (ImGui::Button("Inject##en_inject", { -1.f, 0.f }))
		{
			SimCommand cmd{ .section = CommandSection::CellManagerEvent, .type = CommandType::InjectProtozoa };
			cmd.float_val = m_feed_amount_;
			cmd.bool_val = energy_mode;
			ctx.push(cmd);
		}
		ImGui::PopStyleColor(3);
	}
	ImGui::EndChild();
}