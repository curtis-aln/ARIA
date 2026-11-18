// Genes tab: value formatting and the small drawing helpers shared by the other genes_* files.
#include "genes_ui_common.h"

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace genes_ui
{
	// ─────────────────────────────────────────────────────────────────────────
	//  Value formatting
	// ─────────────────────────────────────────────────────────────────────────

	void format_value(char* buf, const size_t n, const GeneInfo& info, const GnMetric metric,
		const float pheno, const float norm, const float sigma)
	{
		switch (metric)
		{
		case GnMetric::Gene:  std::snprintf(buf, n, "%.3f", norm);  return;
		case GnMetric::Sigma: std::snprintf(buf, n, "%.4f", sigma); return;
		default: break;
		}

		// GnMetric::Value: precision comes from the gene's own display format
		switch (info.fmt)
		{
		case GeneFmt::Whole: std::snprintf(buf, n, "%.0f", pheno); break;
		case GeneFmt::Tenth: std::snprintf(buf, n, "%.1f", pheno); break;
		case GeneFmt::Fine:  std::snprintf(buf, n, "%.4f", pheno); break;
		default:             std::snprintf(buf, n, "%.3f", pheno); break;
		}
	}

	bool derived_text(const GeneInfo& info, const float value, const Maxes& maxes, char* out, const size_t n)
	{
		switch (info.derive)
		{
		case GeneDerive::Period:
			if (std::abs(value) < 1e-6f)
				std::snprintf(out, n, "static, no oscillation");
			else
				std::snprintf(out, n, "period %.1f frames%s", k_two_pi / std::abs(value), value < 0.f ? " (reversed)" : "");
			return true;

			// Absolute amounts: the gene is a fraction of this cell's own cap
		case GeneDerive::EnergyAbs:
			std::snprintf(out, n, "= %.1f energy (max %.0f)", value * maxes.energy, maxes.energy);
			return true;
		case GeneDerive::IntegrityAbs:
			std::snprintf(out, n, "= %.1f integrity (max %.0f)", value * maxes.integrity, maxes.integrity);
			return true;
		case GeneDerive::NutrientsAbs:
			std::snprintf(out, n, "= %.1f nutrients (max %.0f)", value * maxes.nutrients, maxes.nutrients);
			return true;

		case GeneDerive::Percent:
			std::snprintf(out, n, "= %.1f%%", value * 100.f);
			return true;
		case GeneDerive::Degrees:
			std::snprintf(out, n, "= %.0f deg", value * 180.f / k_pi);
			return true;
		case GeneDerive::EffectiveGive:
			std::snprintf(out, n, "effective %.0f%% (sim clamps to 10-90%%)", std::clamp(value, 0.1f, 0.9f) * 100.f);
			return true;
		default:
			return false;
		}
	}

	// Case-insensitive substring search; an empty needle matches everything
	bool icontains(const char* haystack, const char* needle)
	{
		if (!*needle)
			return true;

		const size_t needle_len = std::strlen(needle);
		for (const char* start = haystack; *start; ++start)
		{
			size_t matched = 0;
			while (matched < needle_len && start[matched]
				&& std::tolower(static_cast<unsigned char>(start[matched])) == std::tolower(static_cast<unsigned char>(needle[matched])))
				++matched;
			if (matched == needle_len)
				return true;
		}
		return false;
	}

	// ─────────────────────────────────────────────────────────────────────────
	//  Colours
	// ─────────────────────────────────────────────────────────────────────────

	ImU32 col32(const sf::Color& c, const int alpha)
	{
		return IM_COL32(c.r, c.g, c.b, alpha);
	}

	ImU32 bright32(const sf::Color& c, const int alpha)
	{
		const auto lift = [](const uint8_t v) { return static_cast<int>(v * 0.75f + 64.f); };
		return IM_COL32(lift(c.r), lift(c.g), lift(c.b), alpha);
	}

	ImVec4 bright4(const sf::Color& c, const float alpha)
	{
		const auto lift = [](const uint8_t v) { return (static_cast<float>(v) / 255.f) * 0.75f + 0.25f; };
		return { lift(c.r), lift(c.g), lift(c.b), alpha };
	}

	// Multiplies a packed colour's alpha, leaving RGB alone
	ImU32 scale_alpha(const ImU32 color, const float factor)
	{
		const float alpha = static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFFu) * factor;
		return (color & ~static_cast<ImU32>(IM_COL32_A_MASK))
			| (static_cast<ImU32>(std::clamp(alpha, 0.f, 255.f)) << IM_COL32_A_SHIFT);
	}

	ImU32 group_accent(const int group, const int alpha)
	{
		float r = 0.f, g = 0.f, b = 0.f;
		ImGui::ColorConvertHSVtoRGB(std::fmod(static_cast<float>(group) * 0.137f + 0.55f, 1.f), 0.45f, 0.85f, r, g, b);
		return IM_COL32(static_cast<int>(r * 255.f), static_cast<int>(g * 255.f), static_cast<int>(b * 255.f), alpha);
	}

	// Background of a matrix cell: brighter when its row and / or column is the selected one
	ImU32 slot_bg(const bool row_selected, const bool column_selected)
	{
		if (row_selected && column_selected) return IM_COL32(40, 46, 66, 255);
		if (row_selected)                    return IM_COL32(32, 35, 46, 255);
		if (column_selected)                 return IM_COL32(26, 30, 44, 255);
		return IM_COL32(21, 22, 27, 255);
	}

	// Grey for converged rows, shading to amber as the cells diverge
	ImU32 spread_fill(const float spread)
	{
		const float t = std::clamp(spread * 3.f, 0.f, 1.f);
		const auto mix = [t](const float a, const float b) { return static_cast<int>(a + (b - a) * t); };
		return IM_COL32(mix(90.f, 255.f), mix(100.f, 160.f), mix(125.f, 60.f), mix(70.f, 150.f));
	}

	// ─────────────────────────────────────────────────────────────────────────
	//  Drawing primitives
	// ─────────────────────────────────────────────────────────────────────────

	// Two-colour diagonal chip: outer colour top-left, inner colour bottom-right
	void draw_chip(ImDrawList* dl, const ImVec2 p, const float size, const sf::Color& upper, const sf::Color& lower)
	{
		dl->AddTriangleFilled({ p.x, p.y }, { p.x + size, p.y }, { p.x, p.y + size }, col32(upper, 255));
		dl->AddTriangleFilled({ p.x + size, p.y }, { p.x + size, p.y + size }, { p.x, p.y + size }, col32(lower, 255));
		dl->AddRect(p, { p.x + size, p.y + size }, IM_COL32(0, 0, 0, 140));
	}

	// A chip vertically centred on the current text line. Reserves its width, so call SameLine afterwards.
	void draw_inline_chip(const sf::Color& outer, const sf::Color& inner)
	{
		constexpr float size = 11.f;
		const ImVec2 p = ImGui::GetCursorScreenPos();
		const float line_h = ImGui::GetTextLineHeight();
		draw_chip(ImGui::GetWindowDrawList(), { p.x, p.y + (line_h - size) * 0.5f }, size, outer, inner);
		ImGui::Dummy({ size + 2.f, line_h });
	}

	void draw_fold_arrow(ImDrawList* dl, const ImVec2 p, const bool open)
	{
		const ImU32 color = IM_COL32(170, 176, 195, 255);
		if (open)
			dl->AddTriangleFilled({ p.x, p.y + 1.f }, { p.x + 8.f, p.y + 1.f }, { p.x + 4.f, p.y + 6.f }, color);
		else
			dl->AddTriangleFilled({ p.x + 1.f, p.y - 1.f }, { p.x + 1.f, p.y + 9.f }, { p.x + 6.f, p.y + 4.f }, color);
	}

	// One matrix cell: dark rounded slot, bar filled from the left with a brighter end cap, right-aligned number
	void paint_cell(ImDrawList* dl, const ImVec2 p0, const ImVec2 p1, const ImU32 background,
		const float pos, const ImU32 fill, const char* text, const bool outline)
	{
		constexpr float rounding = 2.f;
		dl->AddRectFilled(p0, p1, background, rounding);

		const float filled_w = (p1.x - p0.x) * std::clamp(pos, 0.f, 1.f);
		if (filled_w > 0.5f)
		{
			dl->AddRectFilled(p0, { p0.x + filled_w, p1.y }, fill, rounding);
			if (filled_w > 4.f)   // the cap marks exactly where the value ends
				dl->AddRectFilled({ p0.x + filled_w - 2.f, p0.y }, { p0.x + filled_w, p1.y }, scale_alpha(fill, 2.2f));
		}

		const ImVec2 text_size = ImGui::CalcTextSize(text);
		dl->AddText({ p1.x - text_size.x - 3.f, p0.y + (p1.y - p0.y - text_size.y) * 0.5f },
			IM_COL32(205, 210, 225, 255), text);

		if (outline)
			dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 200), rounding, 0, 1.f);
	}

	// A 0..1 track with the generation-0 spawn range shaded and a marker at the value.
	// Advances the cursor by `height`.
	void draw_range_bar(ImDrawList* dl, const float width, const float height, const float value_norm,
		const Range& spawn_norm, const ImU32 marker)
	{
		const ImVec2 p0 = ImGui::GetCursorScreenPos();
		const ImVec2 p1{ p0.x + width, p0.y + height };
		dl->AddRectFilled(p0, p1, IM_COL32(22, 24, 30, 255), 3.f);

		const float band_x0 = p0.x + spawn_norm.min * width;
		const float band_x1 = std::max(p0.x + spawn_norm.max * width, band_x0 + 2.f);
		dl->AddRectFilled({ band_x0, p0.y + 2.f }, { band_x1, p1.y - 2.f }, IM_COL32(70, 130, 190, 70), 2.f);

		const float x = p0.x + std::clamp(value_norm, 0.f, 1.f) * width;
		dl->AddRectFilled({ x - 1.5f, p0.y + 1.f }, { x + 1.5f, p1.y - 1.f }, marker, 1.f);

		ImGui::Dummy({ width, height });
	}

	// One line per condition that applies: at a limit, or outside the spawn range
	void draw_flag_lines(const GeneFlags& flags)
	{
		if (flags.at_min)      ImGui::TextColored(k_orange, "At its lower limit");
		if (flags.at_max)      ImGui::TextColored(k_orange, "At its upper limit");
		if (flags.above_spawn) ImGui::TextColored(k_accent, "Above its generation-0 spawn range");
		if (flags.below_spawn) ImGui::TextColored(k_accent, "Below its generation-0 spawn range");
	}

	// ─────────────────────────────────────────────────────────────────────────
	//  Layout helpers
	// ─────────────────────────────────────────────────────────────────────────

	// Dim heading followed by a thin rule that runs to the right edge
	void draw_section_title(const char* title)
	{
		ImGui::Spacing();
		const float width = ImGui::GetContentRegionAvail().x;
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		const ImVec2 text_size = ImGui::CalcTextSize(title);
		ImGui::TextDisabled("%s", title);

		const float rule_y = origin.y + text_size.y * 0.5f;
		ImGui::GetWindowDrawList()->AddLine({ origin.x + text_size.x + 6.f, rule_y }, { origin.x + width, rule_y },
			IM_COL32(60, 64, 80, 255));
	}

	namespace
	{
		// "Label   value" row: dim label, value starting at a fixed column
		void property_row(const char* label, const ImVec4* value_color, const char* fmt, va_list args)
		{
			ImGui::TextDisabled("%s", label);
			ImGui::SameLine(k_prop_label_w);
			if (value_color)
				ImGui::PushStyleColor(ImGuiCol_Text, *value_color);
			ImGui::TextV(fmt, args);
			if (value_color)
				ImGui::PopStyleColor();
		}
	}

	void draw_property(const char* label, const char* fmt, ...)
	{
		va_list args;
		va_start(args, fmt);
		property_row(label, nullptr, fmt, args);
		va_end(args);
	}

	void draw_property_colored(const char* label, const ImVec4& color, const char* fmt, ...)
	{
		va_list args;
		va_start(args, fmt);
		property_row(label, &color, fmt, args);
		va_end(args);
	}

	bool segmented_control(const char* id, const char* const* labels, const int count, int& selected)
	{
		constexpr float h_pad = 8.f;
		constexpr float rounding = 4.f;

		ImDrawList* dl = ImGui::GetWindowDrawList();
		const float height = ImGui::GetFrameHeight();
		bool changed = false;

		ImGui::PushID(id);
		for (int i = 0; i < count; ++i)
		{
			const ImVec2 text_size = ImGui::CalcTextSize(labels[i]);
			const ImVec2 p0 = ImGui::GetCursorScreenPos();
			const ImVec2 p1{ p0.x + text_size.x + h_pad * 2.f, p0.y + height };

			ImGui::PushID(i);
			if (ImGui::InvisibleButton("##segment", { p1.x - p0.x, height }) && selected != i)
			{
				selected = i;
				changed = true;
			}
			const bool hovered = ImGui::IsItemHovered();
			ImGui::PopID();

			// Only the outer ends are rounded so the segments read as one control
			ImDrawFlags corners = ImDrawFlags_RoundCornersNone;
			if (count == 1)            corners = ImDrawFlags_RoundCornersAll;
			else if (i == 0)           corners = ImDrawFlags_RoundCornersLeft;
			else if (i == count - 1)   corners = ImDrawFlags_RoundCornersRight;

			const bool active = selected == i;
			const ImU32 fill = active ? IM_COL32(62, 92, 158, 255)
				: hovered ? IM_COL32(48, 51, 64, 255)
				: IM_COL32(36, 38, 48, 255);
			dl->AddRectFilled(p0, p1, fill, rounding, corners);
			dl->AddRect(p0, p1, IM_COL32(72, 76, 94, 255), rounding, corners);
			dl->AddText({ p0.x + h_pad, p0.y + (height - text_size.y) * 0.5f },
				active ? IM_COL32(240, 244, 255, 255) : IM_COL32(180, 186, 205, 255), labels[i]);

			if (i + 1 < count)
				ImGui::SameLine(0.f, 0.f);
		}
		ImGui::PopID();
		return changed;
	}
}