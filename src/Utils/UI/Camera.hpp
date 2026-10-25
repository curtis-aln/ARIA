#pragma once
#include <SFML/Graphics/View.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/System/Vector2.hpp>


class Camera
{
public:
    sf::View m_view_{};

    float m_pan_friction_ = 8.0f;   // lower = slides longer after release
    float m_pan_lerp_ = 10.0f;  // lower = more lag when starting/changing direction

private:
    using cam_clock = std::chrono::steady_clock;
    cam_clock::time_point m_last_update_ = cam_clock::now();


    sf::RenderWindow* m_window_ = nullptr;

    // zoom
    float m_current_zoom_ = 1.f;
    float m_target_zoom_ = 1.f;
    float m_zoom_strength_ = 0.08f;
    float m_smooth_zoom_speed_ = 8.0f;

    // pan
    sf::Vector2f m_mouse_prev_{};
    sf::Vector2f m_pan_velocity_{};

    bool m_is_panning_ = false;

    sf::Vector2f m_zoom_anchor_{};
    bool m_zoom_anchor_is_mouse_ = true;
    bool m_zoom_has_anchor_ = false;

public:
	float get_current_zoom() const { return m_current_zoom_; }

    explicit Camera(sf::RenderWindow* window_ptr, const float scale = 1.f)
        : m_window_(window_ptr), m_current_zoom_(scale), m_target_zoom_(scale)
    {
        const auto dims = sf::Vector2f(m_window_->getSize());
        m_view_ = sf::View(sf::Rect<float>{{0.f, 0.f}, { dims.x, dims.y }});
        m_view_.setSize(dims / m_current_zoom_);
        update_window_view();
    }

    // call at the start of each frame, before event handling
    void update() 
    {
        const auto now = cam_clock::now();
        const float dt = std::min(std::chrono::duration<float>(now - m_last_update_).count(), 0.1f); // clamp hitches
        m_last_update_ = now;

        const sf::Vector2f mouse_pos = sf::Vector2f(sf::Mouse::getPosition(*m_window_));
        const sf::Vector2f raw_delta = mouse_pos - m_mouse_prev_;
        m_mouse_prev_ = mouse_pos;

        if (dt > 0.f)
        {
            if (m_is_panning_)
            {
                const sf::Vector2f target_vel = raw_delta / dt;               // px/s
                const float a = 1.f - std::exp(-m_pan_lerp_ * dt);            // frame-rate independent lerp
                m_pan_velocity_ += (target_vel - m_pan_velocity_) * a;
            }
            else
            {
                m_pan_velocity_ *= std::exp(-m_pan_friction_ * dt);           // frame-rate independent decay
            }

            m_view_.move(-m_pan_velocity_ * dt / m_current_zoom_);            // px/s * s
        }

        smooth_zoom(dt);
        update_window_view();
    }

    // Instantly sets zoom (and optionally center) with no smoothing/animation.
// Use this once, right after constructing the Camera, to establish the
// starting view — zoom()/set_zoom() are both designed to animate over
// subsequent update() calls, so calling them before your loop starts either
// does nothing visible yet or produces a visible zoom-in on frame 1.
    void set_initial_zoom(const float zoom_level, const std::optional<sf::Vector2f> center = std::nullopt)
    {
        m_current_zoom_ = zoom_level;
        m_target_zoom_ = zoom_level;
        m_zoom_anchor_is_mouse_ = false;
        m_zoom_has_anchor_ = false;

        m_view_.setSize(sf::Vector2f(m_window_->getSize()) / m_current_zoom_);

        if (center)
            m_view_.setCenter(*center);

        update_window_view();
    }

    void begin_pan() { m_is_panning_ = true; }
    void end_pan() { m_is_panning_ = false; }

    void zoom(const float scroll_delta)
    {
        m_zoom_anchor_is_mouse_ = true;
        if (scroll_delta > 0)
            m_target_zoom_ *= (1.0f + m_zoom_strength_);
        else
            m_target_zoom_ *= (1.0f - m_zoom_strength_);
    }

    sf::Vector2f get_world_mouse_pos() const
    {
        return m_window_->mapPixelToCoords(sf::Mouse::getPosition(*m_window_), m_view_);
    }

    template<typename T>
    sf::Vector2f window_pos_to_world_pos(const sf::Vector2<T> window_pos) const
    {
        return m_window_->mapPixelToCoords(static_cast<sf::Vector2i>(window_pos), m_view_);
    }

    template<typename F>
    void call_draw_at_absolute_position(F&& fn)
    {
        const auto saved = m_window_->getView();
        m_window_->setView(m_window_->getDefaultView());
        fn();
        m_window_->setView(saved);
    }

    void update_window_view() const
    {
        m_window_->setView(m_view_);
    }

    // smoothly sets zoom to an absolute level.
    // if 'center' is given, the view also smoothly pans so that world point
    // becomes the new view center. if omitted, current center is left alone.
    void set_zoom(const float zoom_level, const sf::Vector2f center = { 0.f, 0.f })
    {
        m_target_zoom_ = zoom_level;
        m_zoom_anchor_is_mouse_ = false;

        if (center != sf::Vector2f{ 0.f, 0.f })
        {
            m_zoom_anchor_ = center;
            m_zoom_has_anchor_ = true;
        }
        else
        {
            m_zoom_has_anchor_ = false;
        }
    }

private:
    void smooth_zoom(const float dt)
    {
        const bool zoom_settled = std::abs(m_current_zoom_ - m_target_zoom_) < 0.0001f;
        const bool anchor_active = m_zoom_has_anchor_ && !m_zoom_anchor_is_mouse_;

        if (zoom_settled && !anchor_active)
            return;

        if (m_zoom_anchor_is_mouse_)
        {
            if (zoom_settled) return;

            const sf::Vector2f before = get_world_mouse_pos();
            m_current_zoom_ += (m_target_zoom_ - m_current_zoom_) * m_smooth_zoom_speed_ * dt;
            m_view_.setSize(sf::Vector2f(m_window_->getSize()) / m_current_zoom_);
            const sf::Vector2f after = get_world_mouse_pos();
            m_view_.move(before - after);
            return;
        }

        if (!zoom_settled)
        {
            m_current_zoom_ += (m_target_zoom_ - m_current_zoom_) * m_smooth_zoom_speed_ * dt;
            m_view_.setSize(sf::Vector2f(m_window_->getSize()) / m_current_zoom_);
        }

        if (anchor_active)
        {
            const sf::Vector2f current_center = m_view_.getCenter();
            const sf::Vector2f new_center = current_center + (m_zoom_anchor_ - current_center) * m_smooth_zoom_speed_ * dt;
            m_view_.setCenter(new_center);

            if (const sf::Vector2f diff = m_zoom_anchor_ - new_center;
                std::abs(diff.x) < 0.5f && std::abs(diff.y) < 0.5f)
                m_zoom_has_anchor_ = false;
        }
    }
};