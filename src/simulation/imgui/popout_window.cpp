#include "popout_window.h"
#include <imgui-SFML.h>
#include <implot.h>
#include <optional>

namespace
{
	void activate(sf::RenderWindow& w) { static_cast<void>(w.setActive(true)); }
}

bool PopoutWindow::open(sf::RenderWindow& main, unsigned width, unsigned height, const char* title,
	const std::function<void()>& init_hook)
{
	if (m_open_) return true;
	m_main_ = &main;

	// Called mid-frame from the main window, so the current ImGui context is the main one.
	const ImGuiStyle main_style = ImGui::GetStyle();
	ImPlotContext* prev_plot = ImPlot::GetCurrentContext();

#if SFML_VERSION_MAJOR >= 3
	m_win_.create(sf::VideoMode({ width, height }), title);
#else
	m_win_.create(sf::VideoMode(width, height), title);
#endif
	if (!m_win_.isOpen())
	{
		activate(main);
		return false;
	}

	// Two vsync'd windows would each block on the monitor refresh and halve the main loop's rate.
	m_win_.setVerticalSyncEnabled(false);
	m_win_.setFramerateLimit(0);
	m_win_.setPosition(m_has_last_pos_ ? m_last_pos_ : main.getPosition() + sf::Vector2i{ 60, 60 });

	const bool default_font = !init_hook;
	if (!ImGui::SFML::Init(m_win_, default_font))        // creates a new ImGui context and makes it current
	{
		m_win_.close();
		ImGui::SFML::SetCurrentWindow(main);
		activate(main);
		return false;
	}

	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;                            // don't fight the main context over imgui.ini
	io.LogFilename = nullptr;
	ImGui::GetStyle() = main_style;

	m_plot_ctx_ = ImPlot::CreateContext();               // ImPlot keeps per-plot state: give it its own
	if (init_hook) init_hook();

	ImPlot::SetCurrentContext(prev_plot);
	ImGui::SFML::SetCurrentWindow(main);                 // back to the main window's ImGui context
	activate(main);

	m_clock_.restart();
	m_shown_ = true;
	m_open_ = true;
	return true;
}

void PopoutWindow::close()
{
	if (!m_open_) return;

	m_last_pos_ = m_win_.getPosition();
	m_has_last_pos_ = true;

	ImPlotContext* prev_plot = ImPlot::GetCurrentContext();
	if (prev_plot == m_plot_ctx_) prev_plot = nullptr;

	ImGui::SFML::SetCurrentWindow(m_win_);
	ImPlot::DestroyContext(m_plot_ctx_);                 // before the ImGui context it was built on
	m_plot_ctx_ = nullptr;
	ImGui::SFML::Shutdown(m_win_);                       // destroys the pop-out's ImGui context + font texture
	m_win_.close();
	m_open_ = false;

	if (m_main_)
	{
		ImGui::SFML::SetCurrentWindow(*m_main_);
		activate(*m_main_);
	}
	ImPlot::SetCurrentContext(prev_plot);
}

void PopoutWindow::set_visible(bool visible)
{
	if (!m_open_ || visible == m_shown_) return;
	m_win_.setVisible(visible);
	m_shown_ = visible;
}

void PopoutWindow::raise()
{
	if (m_open_) m_win_.requestFocus();
}

bool PopoutWindow::frame(const std::function<void(ImVec2)>& body)
{
	if (!m_open_) return false;

	ImPlotContext* prev_plot = ImPlot::GetCurrentContext();
	activate(m_win_);
	ImGui::SFML::SetCurrentWindow(m_win_);
	ImPlot::SetCurrentContext(m_plot_ctx_);

	// ── Events for the pop-out window ─────────────────────────────────────
	bool closed = false;
#if SFML_VERSION_MAJOR >= 3
	while (const std::optional ev = m_win_.pollEvent())
	{
		ImGui::SFML::ProcessEvent(m_win_, *ev);
		if (ev->is<sf::Event::Closed>())
			closed = true;
		else if (const auto* r = ev->getIf<sf::Event::Resized>())
			m_win_.setView(sf::View(sf::FloatRect({ 0.f, 0.f }, sf::Vector2f(r->size))));
	}
#else
	sf::Event ev;
	while (m_win_.pollEvent(ev))
	{
		ImGui::SFML::ProcessEvent(m_win_, ev);
		if (ev.type == sf::Event::Closed)
			closed = true;
		else if (ev.type == sf::Event::Resized)
			m_win_.setView(sf::View(sf::FloatRect(0.f, 0.f,
				static_cast<float>(ev.size.width), static_cast<float>(ev.size.height))));
	}
#endif

	if (closed)
	{
		ImPlot::SetCurrentContext(prev_plot);
		close();                                          // also restores the main window's contexts
		return false;
	}

	// ── One full ImGui frame ──────────────────────────────────────────────
	sf::Time dt = m_clock_.restart();
	if (dt.asSeconds() <= 0.f) dt = sf::seconds(1.f / 60.f);

	ImGui::SFML::Update(m_win_, dt);

	const auto sz = m_win_.getSize();
	body(ImVec2(static_cast<float>(sz.x), static_cast<float>(sz.y)));

	m_win_.clear(sf::Color(18, 18, 22));
	ImGui::SFML::Render(m_win_);
	m_win_.display();

	// ── Hand everything back to the main window, mid-frame ────────────────
	ImPlot::SetCurrentContext(prev_plot);
	ImGui::SFML::SetCurrentWindow(*m_main_);
	activate(*m_main_);
	return true;
}