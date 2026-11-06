// Energy tab — Cells card: per-cell state cards or heat-strip, sorting, click to select.
#include "energy_ui_common.h"
#include <algorithm>
#include <cstdio>
#include <numeric>
#include <vector>

using namespace en_ui;

namespace
{
	// ── Cell state ──────────────────────────────────────────────────────────
	enum class EnState { Ok, Starving, Damaged, Ready, Digesting, Immortal };

	struct EnStateInfo { const char* tag; const char* full; ImVec4 col; };

	EnStateInfo state_info(EnState s)
	{
		switch (s)
		{
		case EnState::Starving:  return { "STARVE", "Starving",   k_en_state_starving };
		case EnState::Damaged:   return { "HURT",   "Damaged",    k_en_state_damaged };
		case EnState::Ready:     return { "READY",  "Ready",      k_en_state_ready };
		case EnState::Digesting: return { "DIGEST", "Digesting",  k_en_state_digesting };
		case EnState::Immortal:  return { "IMM",    "Immortal",   k_en_state_immortal };
		default:                 return { "OK",     "Healthy",    k_en_state_ok };
		}
	}

	EnState classify(const Cell& c)
	{
		const float e = c.get_energy() / CellSettings::max_energy;
		const float i = c.get_integrity() / CellSettings::max_integrity;
		if (e < k_starve_frac)  return EnState::Starving;
		if (i < k_damaged_frac) return EnState::Damaged;
		if (c.should_reproduce()) return EnState::Ready;
		if (c.total_food_eaten_ > 0 &&
			static_cast<float>(c.time_since_last_ate_) < CellSettings::digestive_time)
			return EnState::Digesting;
		if (c.immortal_) return EnState::Immortal;
		return EnState::Ok;
	}

	void cell_tooltip(int i, const Cell& c, EnState s)
	{
		const EnStateInfo si = state_info(s);
		ImGui::BeginTooltip();
		ImGui::TextColored(si.col, "C%d  %s", i, si.full);
		ImGui::Separator();
		ImGui::Text("Energy     %.2f / %.0f   (repro %.0f)", c.get_energy(), CellSettings::max_energy, static_cast<float>(c.energy_threshold()));
		ImGui::Text("Nutrients  %.2f / %.0f   (repro %.0f)", c.nutrients_, CellSettings::max_nutrients, static_cast<float>(c.nutrients_threshold()));
		ImGui::Text("Integrity  %.2f / %.0f   (repro %.0f)", c.get_integrity(), CellSettings::max_integrity, static_cast<float>(c.integrity_threshold()));
		ImGui::Text("Ate %u  (%llu fr ago)", static_cast<unsigned>(c.total_food_eaten_), static_cast<unsigned long long>(c.time_since_last_ate_));
		ImGui::Text("dE %+.3f   dI %+.3f", c.delta_energy, c.delta_integrity);
		ImGui::TextDisabled("click to select");
		ImGui::EndTooltip();
	}

	// One cell as a compact card: id + state badge on top, E/N/I as three thin stacked bars.
	bool cell_card(int i, const Cell& c, bool selected)
	{
		ImGui::PushID(i);
		ImGui::InvisibleButton("##card", { k_cell_card_w, k_cell_card_h });
		ImGui::PopID();
		const bool clicked = ImGui::IsItemClicked();
		const bool hovered = ImGui::IsItemHovered();
		const ImVec2 p0 = ImGui::GetItemRectMin(), p1 = ImGui::GetItemRectMax();
		ImDrawList* dl = ImGui::GetWindowDrawList();

		const EnState s = classify(c);
		const EnStateInfo si = state_info(s);

		dl->AddRectFilled(p0, p1, u32(k_en_col_cell_bg, hovered ? 1.f : 0.85f), 4.f);
		dl->AddRectFilled(p0, { p0.x + 3.f, p1.y }, u32(si.col), 4.f, ImDrawFlags_RoundCornersLeft);
		dl->AddRect(p0, p1, selected ? u32({ 1.f, 1.f, 1.f, 0.9f }) : u32(k_en_col_card_border), 4.f, 0, selected ? 2.f : 1.f);

		char lbl[16];
		std::snprintf(lbl, sizeof lbl, "C%d", i);
		dl->AddText({ p0.x + 9.f, p0.y + 2.f }, u32({ 0.85f, 0.87f, 0.92f, 1.f }), lbl);
		const ImVec2 ts = ImGui::CalcTextSize(si.tag);
		dl->AddText({ p1.x - ts.x - 5.f, p0.y + 2.f }, u32(si.col), si.tag);

		const float bx0 = p0.x + 9.f, bx1 = p1.x - 6.f, bw = bx1 - bx0;
		auto bar = [&](int row, float f, float thr, const ImVec4& col)
			{
				const float y = p0.y + 19.f + static_cast<float>(row) * 7.f;
				dl->AddRectFilled({ bx0, y }, { bx1, y + 5.f }, u32({ 1.f, 1.f, 1.f, 0.08f }), 2.f);
				if (f > 0.f)
					dl->AddRectFilled({ bx0, y }, { bx0 + bw * f, y + 5.f }, u32(col), 2.f);
				if (thr > 0.f && thr < 1.f)
					dl->AddLine({ bx0 + bw * thr, y - 1.f }, { bx0 + bw * thr, y + 6.f }, u32({ 1.f, 1.f, 1.f, 0.8f }), 1.f);
			};

		const float fe = safe_frac(c.get_energy(), CellSettings::max_energy);
		bar(0, fe, safe_frac(static_cast<float>(c.energy_threshold()), CellSettings::max_energy), fraction_color(fe));
		bar(1, safe_frac(c.nutrients_, CellSettings::max_nutrients),
			safe_frac(static_cast<float>(c.nutrients_threshold()), CellSettings::max_nutrients), k_en_col_nutrients);
		bar(2, safe_frac(c.get_integrity(), CellSettings::max_integrity),
			safe_frac(static_cast<float>(c.integrity_threshold()), CellSettings::max_integrity), k_en_col_integrity);

		if (hovered) cell_tooltip(i, c, s);
		return clicked;
	}

	// One cell as a small colour block (heat-strip mode). metric: 0 energy, 1 nutrients, 2 integrity.
	bool strip_block(int i, const Cell& c, int metric, bool selected)
	{
		ImGui::PushID(i);
		ImGui::InvisibleButton("##blk", { k_strip_block, k_strip_block });
		ImGui::PopID();
		const bool clicked = ImGui::IsItemClicked();
		const bool hovered = ImGui::IsItemHovered();
		const ImVec2 p0 = ImGui::GetItemRectMin(), p1 = ImGui::GetItemRectMax();
		ImDrawList* dl = ImGui::GetWindowDrawList();

		ImVec4 col;
		if (metric == 0)
		{
			const float f = safe_frac(c.get_energy(), CellSettings::max_energy);
			col = fraction_color(f);
		}
		else if (metric == 1)
		{
			const float f = safe_frac(c.nutrients_, CellSettings::max_nutrients);
			col = { k_en_col_nutrients.x, k_en_col_nutrients.y, k_en_col_nutrients.z, 0.15f + 0.85f * f };
		}
		else
		{
			const float f = safe_frac(c.get_integrity(), CellSettings::max_integrity);
			col = { k_en_col_integrity.x, k_en_col_integrity.y, k_en_col_integrity.z, 0.15f + 0.85f * f };
		}

		dl->AddRectFilled(p0, p1, u32(k_en_col_cell_bg), 3.f);
		dl->AddRectFilled(p0, p1, u32(col), 3.f);
		if (selected)
			dl->AddRect(p0, p1, u32({ 1.f, 1.f, 1.f, 0.95f }), 3.f, 0, 2.f);
		else if (hovered)
			dl->AddRect(p0, p1, u32({ 1.f, 1.f, 1.f, 0.5f }), 3.f, 0, 1.f);

		if (hovered) cell_tooltip(i, c, classify(c));
		return clicked;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Cells — state cards (or heat-strip for large organisms), click to select
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_energy_cells(const OrganismTracker& t, float w)
{
	if (begin_card("EN_cells", w))
	{
		const int n = t.cell_count;

		static const char* sorts[] = { "Index", "Energy (low)", "Integrity (low)", "Food (high)" };
		static const char* views[] = { "Auto", "Cards", "Strip" };

		ImGui::SetNextItemWidth(108.f);
		ImGui::Combo("##sort", &m_cell_sort_, sorts, 4);
		ImGui::SetItemTooltip("Sort order");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(64.f);
		ImGui::Combo("##view", &m_cell_view_, views, 3);
		ImGui::SetItemTooltip("View: cards, heat-strip, or auto (strip above %d cells)", k_strip_auto_threshold);

		const bool strip = m_cell_view_ == 2 || (m_cell_view_ == 0 && n > k_strip_auto_threshold);
		if (strip)
		{
			ImGui::RadioButton("E##sm", &m_strip_metric_, 0); ImGui::SameLine();
			ImGui::RadioButton("N##sm", &m_strip_metric_, 1); ImGui::SameLine();
			ImGui::RadioButton("I##sm", &m_strip_metric_, 2);
		}

		// ── Order ────────────────────────────────────────────────────────
		static std::vector<int> order;
		order.resize(static_cast<size_t>(n));
		std::iota(order.begin(), order.end(), 0);
		if (m_cell_sort_ != 0)
		{
			auto key = [&](int i) -> float
				{
					const Cell& c = t.cells[i];
					switch (m_cell_sort_)
					{
					case 1:  return c.get_energy();
					case 2:  return c.get_integrity();
					default: return -static_cast<float>(c.total_food_eaten_);
					}
				};
			std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return key(a) < key(b); });
		}

		const float gap = k_cell_card_gap;

		if (!strip)
		{
			// Cards flow top-to-bottom, then left-to-right; horizontal scroll if they overflow.
			ImGui::BeginChild("##cell_scroll", { -1.f, -1.f }, false, ImGuiWindowFlags_HorizontalScrollbar);
			const float avail_h = ImGui::GetContentRegionAvail().y - ImGui::GetStyle().ScrollbarSize;
			const int per_col = std::max(1, static_cast<int>((avail_h + gap) / (k_cell_card_h + gap)));
			const ImVec2 base = ImGui::GetCursorPos();

			for (int k = 0; k < n; ++k)
			{
				const int i = order[static_cast<size_t>(k)];
				const int col = k / per_col, row = k % per_col;
				ImGui::SetCursorPos({ base.x + static_cast<float>(col) * (k_cell_card_w + gap),
					base.y + static_cast<float>(row) * (k_cell_card_h + gap) });
				const bool sel = !m_sel_is_spring_ && m_sel_cell_idx_ == i;
				if (cell_card(i, t.cells[i], sel))
				{
					m_sel_cell_idx_ = i;
					m_sel_is_spring_ = false;
				}
			}
			ImGui::EndChild();
		}
		else
		{
			ImGui::BeginChild("##cell_scroll", { -1.f, -1.f }, false);
			const float avail_w = ImGui::GetContentRegionAvail().x;
			const int per_row = std::max(1, static_cast<int>((avail_w + 3.f) / (k_strip_block + 3.f)));
			const ImVec2 base = ImGui::GetCursorPos();

			for (int k = 0; k < n; ++k)
			{
				const int i = order[static_cast<size_t>(k)];
				const int col = k % per_row, row = k / per_row;
				ImGui::SetCursorPos({ base.x + static_cast<float>(col) * (k_strip_block + 3.f),
					base.y + static_cast<float>(row) * (k_strip_block + 3.f) });
				const bool sel = !m_sel_is_spring_ && m_sel_cell_idx_ == i;
				if (strip_block(i, t.cells[i], m_strip_metric_, sel))
				{
					m_sel_cell_idx_ = i;
					m_sel_is_spring_ = false;
				}
			}
			ImGui::EndChild();
		}
	}
	ImGui::EndChild();
}