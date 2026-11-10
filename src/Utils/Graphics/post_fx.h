#pragma once
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <imgui.h>

struct PostFxSettings
{
	// Depth of field
	inline static bool  focus_enabled = true;
	inline static float focus_strength = 1.f;   // 0..2 multiplier on blur
	inline static float focus_radius = 0.25f; // 0..0.9 of centre-to-corner that stays sharp
	inline static float focus_max_blur_px = 6.f;    // blur radius at the corners, at 1080p
	inline static bool  focus_tie_zoom = true;   // zoomed in = shallower DoF

	// Vignette
	inline static bool  vignette_enabled = true;
	inline static float vignette_strength = 0.85f;  // 0..1
	inline static float vignette_radius = 0.55f;  // where darkening starts (0 centre, 1 corner)
	inline static float vignette_softness = 0.5f;   // falloff width
};

inline const char* k_post_frag = R"(
uniform sampler2D tex;
uniform vec2  res;
uniform float focus_strength, focus_radius, max_blur;
uniform float vig_strength, vig_radius, vig_soft;

void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    vec2 p  = (uv - 0.5) * vec2(res.x / res.y, 1.0);
    float d = length(p) / length(vec2(0.5 * res.x / res.y, 0.5));   // 0 = centre, 1 = corner

    vec3 col = texture2D(tex, uv).rgb;

    // depth of field: sharp centre, golden-angle disc blur ramping toward the edges
    float defocus = focus_strength * smoothstep(focus_radius, 1.0, d) * max_blur * (res.y / 1080.0);
    if (defocus > 0.3)
    {
        vec3 acc = col;
        for (int i = 0; i < 12; i++)
        {
            float a = float(i) * 2.39996323;
            float r = sqrt((float(i) + 0.5) / 12.0);
            acc += texture2D(tex, uv + vec2(cos(a), sin(a)) * r * defocus / res).rgb;
        }
        col = acc / 13.0;
    }

    // vignette
    col *= 1.0 - vig_strength * smoothstep(vig_radius, vig_radius + vig_soft, d);

    gl_FragColor = vec4(col, 1.0);
}
)";

class PostFx
{
	sf::Shader  shader_;
	sf::Texture frame_;
	bool        ok_ = false;

public:
	bool init()
	{
		ok_ = sf::Shader::isAvailable() && shader_.loadFromMemory(k_post_frag, sf::Shader::Type::Fragment);
		return ok_;
	}

	void apply(sf::RenderWindow& win, float zoom)
	{
		using S = PostFxSettings;
		const bool dof = S::focus_enabled && S::focus_strength > 0.f;
		const bool vig = S::vignette_enabled && S::vignette_strength > 0.f;
		if (!ok_ || (!dof && !vig))
			return;                                   // both off = zero cost

		const sf::Vector2u size = win.getSize();
		if (frame_.getSize() != size && !frame_.resize(size))
			return;
		frame_.update(win);                           // copy the finished world frame

		// Focus knob: higher magnification -> shallower depth of field
		float k = 1.f;
		if (S::focus_tie_zoom)
		{
			const float t = std::clamp((zoom - 0.03f) / 0.97f, 0.f, 1.f);
			k = 0.35f + 1.25f * (t * t * (3.f - 2.f * t));   // 0.35x zoomed out .. 1.6x zoomed in
		}

		shader_.setUniform("tex", sf::Shader::CurrentTexture);
		shader_.setUniform("res", sf::Vector2f(size));
		shader_.setUniform("focus_strength", dof ? S::focus_strength * k : 0.f);
		shader_.setUniform("focus_radius", S::focus_radius);
		shader_.setUniform("max_blur", S::focus_max_blur_px);
		shader_.setUniform("vig_strength", vig ? S::vignette_strength : 0.f);
		shader_.setUniform("vig_radius", S::vignette_radius);
		shader_.setUniform("vig_soft", std::max(S::vignette_softness, 0.01f));

		const sf::View saved = win.getView();
		win.setView(win.getDefaultView());

		sf::RenderStates rs;
		rs.shader = &shader_;
		rs.blendMode = sf::BlendNone;                 // overwrite, don't blend with the unprocessed frame
		win.draw(sf::Sprite(frame_), rs);

		win.setView(saved);
	}
};

// Call from inside any tab's draw()
inline void draw_post_fx_ui()
{
	using S = PostFxSettings;
	ImGui::SeparatorText("Microscope");

	ImGui::Checkbox("Focus (depth of field)", &S::focus_enabled);
	ImGui::BeginDisabled(!S::focus_enabled);
	ImGui::SliderFloat("Blur strength", &S::focus_strength, 0.f, 2.f);
	ImGui::SliderFloat("Sharp radius", &S::focus_radius, 0.f, 0.9f);
	ImGui::SliderFloat("Max blur (px)", &S::focus_max_blur_px, 0.f, 20.f);
	ImGui::Checkbox("Tie to zoom", &S::focus_tie_zoom);
	ImGui::EndDisabled();

	ImGui::Checkbox("Vignette", &S::vignette_enabled);
	ImGui::BeginDisabled(!S::vignette_enabled);
	ImGui::SliderFloat("Vignette strength", &S::vignette_strength, 0.f, 1.f);
	ImGui::SliderFloat("Vignette radius", &S::vignette_radius, 0.f, 1.f);
	ImGui::SliderFloat("Vignette softness", &S::vignette_softness, 0.05f, 1.f);
	ImGui::EndDisabled();
}