#pragma once
#include "i_tab.h"
#include <vector>

class GraphsTab : public ITab
{
public:
    const char* label() const override { return "Graphs"; }
    void        draw(const SimSnapshot& snap, ImGuiContext& ctx)   override;

private:
    // ── Shared toolbar state ──────────────────────────────────────────────
    float m_scroll_window_ = 160.f;
    bool  m_recording_ = false;
    float m_record_start_ = 0.f;

    // Freeze the X window while the user hovers so they can inspect values.
    bool  m_hover_paused_ = false;
    float m_paused_x_max_ = 0.f;

    // ── Population tab toggles ────────────────────────────────────────────
    bool m_show_protozoa_ = true;
    bool m_show_food_ = true;
    bool m_show_total_ = true;
    bool m_show_bands_ = true;

    // ── Band cache — recomputed only when the history grows or shrinks ────
    struct BandCache
    {
        std::vector<float> plo, phi;   // protozoa band
        std::vector<float> flo, fhi;   // food band
        size_t             valid_for_n = 0;

        // Refresh only when the sample count has changed.
        void refresh(const struct PopulationHistory& h, bool need_protozoa, bool need_food);
    };
    BandCache m_band_cache_;

    // ── Misc tab toggles ──────────────────────────────────────────────────
    bool m_show_mut_rate_ = true;
    bool m_show_mut_range_ = true;
    bool m_show_offspring_ = true;
    bool m_show_lifetime_ = false;
    bool m_show_avg_cells_ = false;
    bool m_show_avg_springs_ = false;
    bool m_show_avg_energy_ = false;

    // When true, force-refit both Y axes on the next misc plot frame.
    bool m_refit_misc_ = false;

    // ── Private draw helpers ──────────────────────────────────────────────
    void draw_shared_toolbar(const SimSnapshot& snapshot);
    void draw_population_tab(const SimSnapshot& snapshot);
    void draw_event_markers(const SimSnapshot& snapshot, float x_min, float x_max, float y_top);
    void draw_record_region(float x_max, float y_top);
    void draw_generations_tab(const SimSnapshot& snapshot);
    void draw_misc_tab(const SimSnapshot& snapshot);

    // Compute the [min, max] of `data` for entries whose matching time falls
    // within [x_min, x_max].  Returns false if no visible data exists.
    static bool visible_range(const std::vector<float>& times,
        const std::vector<float>& data,
        float x_min, float x_max,
        float& out_lo, float& out_hi);
};