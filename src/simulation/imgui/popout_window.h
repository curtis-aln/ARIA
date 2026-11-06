#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  PopoutWindow — a second OS window (sf::RenderWindow) with its own ImGui and ImPlot
//  contexts, so a panel can leave the main SFML window.
//
//  Why not ImGui "viewports"? ImGui-SFML has no multi-viewport platform backend. ImGui-SFML *does*
//  support several windows, each with its own ImGui context, so that's what this uses.
//
//  Works with SFML 2.6 + ImGui-SFML 2.6 and SFML 3 + ImGui-SFML 3 (see SFML_VERSION_MAJOR branches).
// ─────────────────────────────────────────────────────────────────────────────
#include <SFML/Config.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <functional>
#include <imgui.h>

struct ImPlotContext;

class PopoutWindow
{
public:
	~PopoutWindow() { close(); }

	// Opens the OS window and creates its ImGui/ImPlot contexts. `main` must be the window ImGui-SFML was
	// initialised with. `init_hook` (optional) runs with the new ImGui context current: use it to load
	// fonts (then call ImGui::SFML::UpdateFontTexture()). If no hook is given the default font is used.
	// The main window's ImGui style is copied automatically.
	bool open(sf::RenderWindow& main, unsigned width, unsigned height, const char* title,
		const std::function<void()>& init_hook = {});

	// Destroys the contexts and the OS window. Safe to call when already closed.
	// Must be called BEFORE ImGui::SFML::Shutdown() at app exit.
	void close();

	bool is_open() const { return m_open_; }
	void set_visible(bool visible);
	void raise();

	// Runs one complete ImGui frame in the pop-out. `body(size)` is called between NewFrame and Render;
	// `size` is the OS window's client size. Returns false if the user closed the OS window this frame
	// (the pop-out has then already been torn down). Call from the middle of the main window's frame:
	// the main ImGui / ImPlot / ImGui-SFML contexts are restored before returning.
	bool frame(const std::function<void(ImVec2 size)>& body);

private:
	sf::RenderWindow  m_win_;
	sf::RenderWindow* m_main_ = nullptr;
	ImPlotContext* m_plot_ctx_ = nullptr;
	sf::Clock         m_clock_;
	sf::Vector2i      m_last_pos_{ 0, 0 };
	bool              m_has_last_pos_ = false;
	bool              m_open_ = false;
	bool              m_shown_ = true;
};