#pragma once
#include <algorithm>
#include <deque>
#include <functional>
#include <imgui.h>
#include <string>
#include <utility>

// Modal warning dialog with Continue / Cancel. Header-only, queues if several are raised at once.
//
//   WarningPopup warnings;   // one instance, alongside the main ImGui context
//
//   warnings.show("Reset world", "Every organism will be deleted.\nThis can't be undone.",
//                 [&] { ctx.push({ ... }); },                 // on continue
//                 {},                                         // on cancel (optional)
//                 WarningPopup::Severity::Danger);
//
//   warnings.draw();   // once per frame, main context, between ImGui::SFML::Update and Render
//
// Callbacks run inside draw() on the UI thread, so they can call ctx.push(...) directly.
// Esc = cancel. Enter = continue (not for Danger, where you have to click it).
class WarningPopup
{
public:
	enum class Severity { Info, Warning, Danger };

	struct Request
	{
		std::string title;
		std::string message;
		std::function<void()> on_continue;
		std::function<void()> on_cancel;          // optional
		std::string continue_label = "Continue";
		std::string cancel_label = "Cancel";
		Severity severity = Severity::Warning;
	};

	void show(Request req) { m_queue_.push_back(std::move(req)); }

	void show(std::string title, std::string message, std::function<void()> on_continue,
		std::function<void()> on_cancel = {}, const Severity severity = Severity::Warning)
	{
		Request r;
		r.title = std::move(title);
		r.message = std::move(message);
		r.on_continue = std::move(on_continue);
		r.on_cancel = std::move(on_cancel);
		r.severity = severity;
		show(std::move(r));
	}

	bool is_open() const { return !m_queue_.empty(); }

	void draw()
	{
		if (m_queue_.empty()) return;

		constexpr const char* k_id = "##WarningPopup";
		constexpr float k_width = 400.f;
		constexpr float k_icon = 44.f;

		const Request& req = m_queue_.front();
		const ImVec4 acc = accent(req.severity);
		const bool danger = req.severity == Severity::Danger;

		if (!m_opened_)
		{
			ImGui::OpenPopup(k_id);
			m_opened_ = true;
			m_open_time_ = ImGui::GetTime();
		}

		// Fade + slight rise on open
		const float age = static_cast<float>(ImGui::GetTime() - m_open_time_);
		const float t = std::clamp(age / 0.18f, 0.f, 1.f);
		const float e = 1.f - (1.f - t) * (1.f - t);

		const ImVec2 centre = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos({ centre.x, centre.y + (1.f - e) * 10.f }, ImGuiCond_Always, { 0.5f, 0.5f });
		ImGui::SetNextWindowSize({ k_width, 0.f }, ImGuiCond_Always);   // y = 0 -> auto-fit height

		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, e);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 22.f, 20.f });
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 8.f, 10.f });
		ImGui::PushStyleColor(ImGuiCol_PopupBg, { 0.085f, 0.090f, 0.115f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_Border, { 1.f, 1.f, 1.f, 0.09f });
		ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, { 0.f, 0.f, 0.f, 0.60f });

		enum class Result { None, Continue, Cancel };
		Result result = Result::None;

		constexpr ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;

		if (ImGui::BeginPopupModal(k_id, nullptr, flags))
		{
			ImDrawList* dl = ImGui::GetWindowDrawList();
			const ImVec2 wp = ImGui::GetWindowPos();
			const float ww = ImGui::GetWindowWidth();

			// Accent line along the top edge, fading out at both ends
			{
				const ImU32 clear = u32(acc, 0.f), solid = u32(acc, 1.f);
				const float x0 = wp.x + 12.f, x1 = wp.x + ww - 12.f, xm = (x0 + x1) * 0.5f;
				dl->AddRectFilledMultiColor({ x0, wp.y + 1.f }, { xm, wp.y + 3.f }, clear, solid, solid, clear);
				dl->AddRectFilledMultiColor({ xm, wp.y + 1.f }, { x1, wp.y + 3.f }, solid, clear, clear, solid);
			}

			// Icon badge + title
			const ImVec2 p = ImGui::GetCursorScreenPos();
			const ImVec2 c = { p.x + k_icon * 0.5f, p.y + k_icon * 0.5f };
			dl->AddCircleFilled(c, k_icon * 0.5f, u32(acc, 0.14f), 40);
			dl->AddCircle(c, k_icon * 0.5f, u32(acc, 0.35f), 40, 1.f);
			draw_icon(dl, c, 10.f, req.severity, u32(acc, 1.f));

			const float fs = ImGui::GetFontSize() * 1.25f;
			dl->AddText(ImGui::GetFont(), fs, { p.x + k_icon + 14.f, p.y + (k_icon - fs) * 0.5f },
				IM_COL32(236, 239, 248, 255), req.title.c_str());
			ImGui::Dummy({ 1.f, k_icon });

			// Message
			ImGui::PushStyleColor(ImGuiCol_Text, { 0.70f, 0.73f, 0.80f, 1.f });
			ImGui::PushTextWrapPos(0.f);
			ImGui::TextUnformatted(req.message.c_str());
			ImGui::PopTextWrapPos();
			ImGui::PopStyleColor();

			ImGui::Spacing();
			ImGui::PushStyleColor(ImGuiCol_Separator, { 1.f, 1.f, 1.f, 0.07f });
			ImGui::Separator();
			ImGui::PopStyleColor();

			// Buttons, right-aligned: [Cancel] [Continue]
			constexpr float bw = 112.f, bh = 32.f;
			const ImGuiStyle& st = ImGui::GetStyle();
			ImGui::SetCursorPosX(ww - st.WindowPadding.x - bw * 2.f - st.ItemSpacing.x);

			ImGui::PushStyleColor(ImGuiCol_Button, { 0.18f, 0.19f, 0.23f, 1.f });
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.24f, 0.25f, 0.30f, 1.f });
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.30f, 0.31f, 0.37f, 1.f });
			if (ImGui::Button((req.cancel_label + "##wp_cancel").c_str(), { bw, bh }))
				result = Result::Cancel;
			if (danger) ImGui::SetItemDefaultFocus();
			ImGui::PopStyleColor(3);

			ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_Button, { acc.x * 0.62f, acc.y * 0.62f, acc.z * 0.62f, 1.f });
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { acc.x * 0.80f, acc.y * 0.80f, acc.z * 0.80f, 1.f });
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, { acc.x * 0.95f, acc.y * 0.95f, acc.z * 0.95f, 1.f });
			if (ImGui::Button((req.continue_label + "##wp_continue").c_str(), { bw, bh }))
				result = Result::Continue;
			if (!danger) ImGui::SetItemDefaultFocus();
			ImGui::PopStyleColor(3);

			// Keyboard (ignored for the first moments so the key that opened it can't dismiss it)
			if (age > 0.15f && result == Result::None)
			{
				if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
					result = Result::Cancel;
				else if (!danger && ImGui::IsKeyPressed(ImGuiKey_Enter, false))
					result = Result::Continue;
			}

			if (result != Result::None)
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}
		else
			result = Result::Cancel;   // popup got closed from elsewhere

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(6);

		if (result != Result::None)
		{
			// Pop before invoking, so a callback can safely queue another warning.
			Request done = std::move(m_queue_.front());
			m_queue_.pop_front();
			m_opened_ = false;

			const auto& cb = result == Result::Continue ? done.on_continue : done.on_cancel;
			if (cb) cb();
		}
	}

private:
	static ImVec4 accent(const Severity s)
	{
		switch (s)
		{
		case Severity::Info:    return { 0.35f, 0.65f, 1.00f, 1.f };
		case Severity::Danger:  return { 1.00f, 0.38f, 0.38f, 1.f };
		default:                return { 1.00f, 0.72f, 0.25f, 1.f };
		}
	}

	static ImU32 u32(const ImVec4& c, const float a)
	{
		return ImGui::ColorConvertFloat4ToU32({ c.x, c.y, c.z, a });
	}

	// Info: "i" in a circle. Warning: "!" in a triangle. Danger: "!" in a circle.
	static void draw_icon(ImDrawList* dl, const ImVec2 c, const float r, const Severity s, const ImU32 col)
	{
		constexpr float th = 2.4f, dot = 1.7f;
		switch (s)
		{
		case Severity::Info:
			dl->AddCircle(c, r, col, 28, th * 0.8f);
			dl->AddCircleFilled({ c.x, c.y - r * 0.42f }, dot, col, 10);
			dl->AddLine({ c.x, c.y - r * 0.10f }, { c.x, c.y + r * 0.45f }, col, th);
			break;
		case Severity::Warning:
			dl->AddTriangle({ c.x, c.y - r }, { c.x - r * 1.05f, c.y + r * 0.8f }, { c.x + r * 1.05f, c.y + r * 0.8f },
				col, th * 0.8f);
			dl->AddLine({ c.x, c.y - r * 0.30f }, { c.x, c.y + r * 0.20f }, col, th);
			dl->AddCircleFilled({ c.x, c.y + r * 0.52f }, dot, col, 10);
			break;
		case Severity::Danger:
			dl->AddCircle(c, r, col, 28, th * 0.8f);
			dl->AddLine({ c.x, c.y - r * 0.50f }, { c.x, c.y + r * 0.10f }, col, th);
			dl->AddCircleFilled({ c.x, c.y + r * 0.45f }, dot, col, 10);
			break;
		}
	}

	std::deque<Request> m_queue_;
	bool   m_opened_ = false;
	double m_open_time_ = 0.0;
};