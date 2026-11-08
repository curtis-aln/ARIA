#pragma once
#include <imgui.h>
#include <implot.h>

inline void apply_aria_theme()
{
	ImGuiStyle& s = ImGui::GetStyle();

	// ── Vars (previously the 10 PushStyleVar calls) ──
	s.WindowRounding = 7.f;
	s.FrameRounding = 4.f;
	s.GrabRounding = 4.f;
	s.TabRounding = 4.f;
	s.ChildRounding = 5.f;
	s.ScrollbarRounding = 6.f;
	s.ScrollbarSize = 9.f;
	s.ItemSpacing = { 6.f, 5.f };
	s.WindowPadding = { 11.f, 9.f };
	s.FramePadding = { 5.f, 3.f };

	// ── New vars ──
	s.PopupRounding = 5.f;
	s.PopupBorderSize = 1.f;
	s.ChildBorderSize = 1.f;
	s.DisabledAlpha = 0.45f;
	s.ItemInnerSpacing = { 5.f, 4.f };
	s.SelectableTextAlign = { 0.f, 0.5f };
	s.SeparatorTextBorderSize = 1.f;
	s.SeparatorTextAlign = { 0.f, 0.5f };
	s.SeparatorTextPadding = { 10.f, 2.f };
#if IMGUI_VERSION_NUM >= 19100
	s.TabBarOverlineSize = 2.f;
#endif
#if IMGUI_VERSION_NUM >= 19190
	s.ScrollbarPadding = 2.f;
#endif

	// ── Colors (previously the 24 PushStyleColor calls) ──
	ImVec4* c = s.Colors;
	c[ImGuiCol_WindowBg] = { 0.07f, 0.07f, 0.10f, 0.95f };
	c[ImGuiCol_Border] = { 0.20f, 0.20f, 0.28f, 0.60f };
	c[ImGuiCol_Tab] = { 0.11f, 0.11f, 0.17f, 1.00f };
	c[ImGuiCol_TabHovered] = { 0.22f, 0.30f, 0.45f, 1.00f };
	c[ImGuiCol_TabSelected] = { 0.18f, 0.36f, 0.58f, 1.00f };
	c[ImGuiCol_Separator] = { 0.22f, 0.22f, 0.30f, 0.80f };
	c[ImGuiCol_SliderGrab] = { 0.35f, 0.62f, 0.92f, 1.00f };
	c[ImGuiCol_SliderGrabActive] = { 0.50f, 0.75f, 1.00f, 1.00f };
	c[ImGuiCol_FrameBg] = { 0.12f, 0.12f, 0.18f, 1.00f };
	c[ImGuiCol_FrameBgHovered] = { 0.18f, 0.18f, 0.28f, 1.00f };
	c[ImGuiCol_FrameBgActive] = { 0.22f, 0.22f, 0.34f, 1.00f };
	c[ImGuiCol_CheckMark] = { 0.30f, 0.88f, 0.42f, 1.00f };
	c[ImGuiCol_Button] = { 0.14f, 0.14f, 0.22f, 1.00f };
	c[ImGuiCol_ButtonHovered] = { 0.22f, 0.28f, 0.44f, 1.00f };
	c[ImGuiCol_ButtonActive] = { 0.28f, 0.45f, 0.70f, 1.00f };
	c[ImGuiCol_Header] = { 0.14f, 0.28f, 0.48f, 0.55f };
	c[ImGuiCol_HeaderHovered] = { 0.20f, 0.36f, 0.58f, 0.80f };
	c[ImGuiCol_HeaderActive] = { 0.26f, 0.44f, 0.70f, 1.00f };
	c[ImGuiCol_PopupBg] = { 0.09f, 0.09f, 0.13f, 0.97f };
	c[ImGuiCol_ScrollbarBg] = { 0.09f, 0.09f, 0.13f, 0.60f };
	c[ImGuiCol_ScrollbarGrab] = { 0.22f, 0.30f, 0.45f, 1.00f };
	c[ImGuiCol_ScrollbarGrabHovered] = { 0.30f, 0.42f, 0.62f, 1.00f };
	c[ImGuiCol_ScrollbarGrabActive] = { 0.35f, 0.62f, 0.92f, 1.00f };
	c[ImGuiCol_TextDisabled] = { 0.52f, 0.57f, 0.70f, 1.00f };

	// ── New colors ──
	c[ImGuiCol_ChildBg] = { 0.09f, 0.09f, 0.13f, 0.60f };
	c[ImGuiCol_PlotHistogram] = { 0.30f, 0.75f, 0.45f, 1.00f };  // ProgressBar fill
	c[ImGuiCol_PlotHistogramHovered] = { 0.40f, 0.85f, 0.55f, 1.00f };
	c[ImGuiCol_TextSelectedBg] = { 0.18f, 0.36f, 0.58f, 0.60f };
	c[ImGuiCol_TabSelectedOverline] = { 0.35f, 0.62f, 0.92f, 1.00f };
	c[ImGuiCol_TableHeaderBg] = { 0.11f, 0.11f, 0.17f, 1.00f };
	c[ImGuiCol_TableBorderStrong] = { 0.20f, 0.20f, 0.28f, 1.00f };
	c[ImGuiCol_TableBorderLight] = { 0.16f, 0.16f, 0.23f, 1.00f };
	c[ImGuiCol_TableRowBg] = { 0.00f, 0.00f, 0.00f, 0.00f };
	c[ImGuiCol_TableRowBgAlt] = { 1.00f, 1.00f, 1.00f, 0.03f };
#if IMGUI_VERSION_NUM >= 19200
	c[ImGuiCol_InputTextCursor] = { 0.72f, 0.84f, 1.00f, 1.00f };
#endif
}

void apply_aria_implot_theme()
{
	ImPlotStyle& ps = ImPlot::GetStyle();
	ps.PlotBorderSize = 1.f;
	ps.Colors[ImPlotCol_FrameBg] = { 0.07f, 0.07f, 0.10f, 1.00f };
	ps.Colors[ImPlotCol_PlotBg] = { 0.09f, 0.09f, 0.13f, 1.00f };
	ps.Colors[ImPlotCol_PlotBorder] = { 0.20f, 0.20f, 0.28f, 0.60f };
	ps.Colors[ImPlotCol_LegendBg] = { 0.09f, 0.09f, 0.13f, 0.90f };
	ps.Colors[ImPlotCol_LegendBorder] = { 0.20f, 0.20f, 0.28f, 0.60f };
	ps.Colors[ImPlotCol_AxisGrid] = { 1.00f, 1.00f, 1.00f, 0.08f };
	ps.Colors[ImPlotCol_AxisText] = { 0.72f, 0.84f, 1.00f, 1.00f };
}