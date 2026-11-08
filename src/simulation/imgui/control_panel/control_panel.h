#pragma once
#include "../../context/sim_snapshot.h"
#include "../misc/popout_window.h"
#include "../tabs/i_tab.h"
#include <functional>
#include <memory>
#include <simulation/context/sim_command.h>
#include <vector>


class ControlPanel
{
public:
	ControlPanel();
	~ControlPanel();

	// Call once with the sf::RenderWindow that ImGui-SFML was initialised with.
	// Without it the "Pop out" button is disabled.
	void set_main_window(sf::RenderWindow& main);

	// Optional: runs with the pop-out's ImGui context current, right after it's created.
	// Use it to load the same fonts as the main window (then call ImGui::SFML::UpdateFontTexture()).
	void set_popout_init(std::function<void()> fn);

	void draw(const SimSnapshot& snap, ImGuiContext& ctx, float dt);

	// Request that a specific tab become active on the next draw() call.
	void select_tab(const char* label);

	// Move the panel into its own OS window / snap it back into the main window.
	void set_popped_out(bool popped_out) { m_popped_out_ = popped_out; }
	bool is_popped_out() const { return m_popped_out_; }

	// Closes the pop-out OS window if open. Call before ImGui::SFML::Shutdown() if the ControlPanel
	// outlives it (the destructor does this too).
	void close_popout();

private:
	void draw_docked(const SimSnapshot& snap, ImGuiContext& ctx);
	void draw_popped_out(const SimSnapshot& snap, ImGuiContext& ctx);
	void draw_header();                                              // title + minimize / pop out / dock / hide
	void draw_tabs(const SimSnapshot& snap, ImGuiContext& ctx);      // tab bar + active tab (same in both modes)

	std::vector<std::unique_ptr<ITab>> m_tabs_;
	const char* m_pending_tab_label_ = nullptr;

	bool m_collapsed_ = false;
	bool m_visible_ = true;

	// ── Pop-out ───────────────────────────────────────────────────────────
	bool              m_popped_out_ = false;
	sf::RenderWindow* m_main_ = nullptr;
	PopoutWindow      m_popout_;
	std::function<void()> m_popout_init_;
};