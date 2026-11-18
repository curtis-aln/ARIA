// Energy tab: Feed card (mode toggle, amount + presets, "after inject" preview).
#include "energy_ui_common.h"
#include <cmath>
#include <cstdio>

using namespace en_ui;

namespace
{
	// ── Feed modes ───────────────────────────────────────────────────────────
	// Values are stored in OrganismTab::m_feed_mode_.
	constexpr int k_feed_mode_energy = 0;
	constexpr int k_feed_mode_nutrients = 1;

	// ── Tunables ─────────────────────────────────────────────────────────────
	constexpr float k_feed_amount_min = 1.f;
	constexpr float k_feed_amount_max = 500.f;
	constexpr float k_preview_bar_h = 10.f;
	constexpr float k_feed_presets[4] = { 10.f, 50.f, 100.f, 500.f };

	constexpr ImVec4 k_energy_mode_button_color = { 0.20f, 0.55f, 0.25f, 1.f };
	constexpr ImVec4 k_nutrient_mode_button_color = { 0.15f, 0.40f, 0.75f, 1.f };
	constexpr ImVec4 k_inactive_button_color = { 0.20f, 0.21f, 0.25f, 1.f };

	// ── Widgets ──────────────────────────────────────────────────────────────
	// Button that is tinted when its mode is active; hover and press brighten the base colour.
	bool draw_segmented_button(const char* label, const bool active, const ImVec4& active_color, const ImVec2& size)
	{
		const ImVec4 base = active ? active_color : k_inactive_button_color;
		ImGui::PushStyleColor(ImGuiCol_Button, base);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ base.x + 0.08f, base.y + 0.08f, base.z + 0.08f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ base.x + 0.14f, base.y + 0.14f, base.z + 0.14f, 1.f });
		const bool pressed = ImGui::Button(label, size);
		ImGui::PopStyleColor(3);
		return pressed;
	}

	// ── Inject preview ───────────────────────────────────────────────────────
	// What the organism's energy or nutrient total would be after pressing Inject.
	struct InjectPreview
	{
		float  current;            // organism total right now
		float  after;              // organism total after the injection
		float  capacity;           // organism capacity for this resource
		bool   exceeds_capacity;
		ImVec4 mode_color;         // colour of the selected resource
	};

	InjectPreview compute_inject_preview(const bool energy_mode, const float amount,
		const EnergyStats& stats, const OrganismTracker& tracker)
	{
		InjectPreview preview{};
		preview.current = energy_mode ? tracker.total_energy : tracker.total_nutrients;
		preview.capacity = energy_mode ? tracker.max_energy : tracker.max_nutrients;

		// The injected amount is either given to each cell or split across the whole organism
		const float total_added = k_inject_is_per_cell ? amount * static_cast<float>(stats.cell_count) : amount;
		preview.after = preview.current + total_added;
		preview.exceeds_capacity = preview.after > preview.capacity;
		preview.mode_color = energy_mode ? k_en_col_energy_line : k_en_col_nutrients;
		return preview;
	}

	// Thin bar: solid up to the current level, faded up to the level after injecting
	// (amber if that overflows the capacity).
	void draw_preview_bar(const InjectPreview& preview)
	{
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		const float  bar_w = ImGui::GetContentRegionAvail().x;

		const float current_fill = safe_frac(preview.current, preview.capacity);
		const float after_fill = safe_frac(preview.after, preview.capacity);
		const ImVec4 after_color = preview.exceeds_capacity ? k_en_col_warn : preview.mode_color;

		draw_list->AddRectFilled(origin, { origin.x + bar_w, origin.y + k_preview_bar_h },
			u32({ 1.f, 1.f, 1.f, 0.08f }), 3.f);                                          // track
		draw_list->AddRectFilled(origin, { origin.x + bar_w * after_fill, origin.y + k_preview_bar_h },
			u32(after_color, 0.45f), 3.f);                                                // after inject
		draw_list->AddRectFilled(origin, { origin.x + bar_w * current_fill, origin.y + k_preview_bar_h },
			u32(preview.mode_color), 3.f);                                                // now
		ImGui::Dummy({ bar_w, k_preview_bar_h });
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Feed card: mode toggle, amount + presets, "after inject" preview
// ─────────────────────────────────────────────────────────────────────────────
void OrganismTab::draw_energy_feed(ImGuiContext& ctx, const EnergyStats& stats, const OrganismTracker& tracker)
{
	if (begin_card("EN_feed", k_en_feed_w))
	{
		ImGui::TextDisabled("Feed");
		ImGui::Separator();
		ImGui::Spacing();

		// Two equal columns for the mode buttons and the preset grid
		const float item_spacing = ImGui::GetStyle().ItemSpacing.x;
		const float half_width = std::floor((ImGui::GetContentRegionAvail().x - item_spacing) * 0.5f);

		// Mode: feed energy or nutrients
		if (draw_segmented_button("Energy##fmode", m_feed_mode_ == k_feed_mode_energy,
			k_energy_mode_button_color, { half_width, 0.f }))
			m_feed_mode_ = k_feed_mode_energy;
		ImGui::SameLine();
		if (draw_segmented_button("Nutrients##fmode", m_feed_mode_ == k_feed_mode_nutrients,
			k_nutrient_mode_button_color, { half_width, 0.f }))
			m_feed_mode_ = k_feed_mode_nutrients;

		// Amount: slider plus preset buttons in two columns
		ImGui::Spacing();
		ImGui::SetNextItemWidth(-1.f);
		ImGui::SliderFloat("##feed_amount", &m_feed_amount_, k_feed_amount_min, k_feed_amount_max, "Amount = %.0f");

		const int preset_count = static_cast<int>(std::size(k_feed_presets));
		for (int i = 0; i < preset_count; ++i)
		{
			if (i % 2 != 0)
				ImGui::SameLine();
			char label[16];
			std::snprintf(label, sizeof label, "%.0f##pre%d", k_feed_presets[i], i);
			if (ImGui::Button(label, { half_width, 0.f }))
				m_feed_amount_ = k_feed_presets[i];
		}

		// Preview of the totals after injecting
		const bool energy_mode = m_feed_mode_ == k_feed_mode_energy;
		const InjectPreview preview = compute_inject_preview(energy_mode, m_feed_amount_, stats, tracker);

		ImGui::Spacing();
		ImGui::TextDisabled("After inject");
		draw_preview_bar(preview);
		ImGui::TextColored(preview.exceeds_capacity ? k_en_col_warn : k_en_col_dim,
			"%.0f -> %.0f / %.0f", preview.current, preview.after, preview.capacity);
		if (preview.exceeds_capacity)
			ImGui::SetItemTooltip("Exceeds the organism maximum");

		// Inject button (green), sends the command to the sim
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.18f, 0.45f, 0.22f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.25f, 0.58f, 0.30f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.32f, 0.70f, 0.38f, 1.f });
		if (ImGui::Button("Inject##en_inject", { -1.f, 0.f }))
		{
			SimCommand command{ .section = CommandSection::CellManagerEvent, .type = CommandType::InjectProtozoa };
			command.float_val = m_feed_amount_;
			command.bool_val = energy_mode;
			ctx.push(command);
		}
		ImGui::PopStyleColor(3);
	}
	ImGui::EndChild();
}