#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <numeric>
#include <cstdint>
#include "imgui.h"

struct PopulationEvent
{
    float       time = 0.f;
    std::string label = {};
    ImVec4      color = { 1.f, 0.8f, 0.2f, 1.f };
};

// ── Misc per-frame averages ───────────────────────────────────────────────────
struct MiscSeries
{
    std::vector<float> mut_rate = {};
    std::vector<float> mut_range = {};
    std::vector<float> avg_offspring = {};
    std::vector<float> avg_lifetime = {};
    std::vector<float> avg_cells = {};
    std::vector<float> avg_springs = {};
    std::vector<float> avg_energy = {};

    void push(float mr, float mrng, float ao, float al, float ac, float asp, float ae)
    {
        mut_rate.push_back(mr);
        mut_range.push_back(mrng);
        avg_offspring.push_back(ao);
        avg_lifetime.push_back(al);
        avg_cells.push_back(ac);
        avg_springs.push_back(asp);
        avg_energy.push_back(ae);
    }

    // Drop the first `n` elements from every series.
    void evict(size_t n)
    {
        auto drop = [n](std::vector<float>& v)
            {
                if (v.size() >= n) v.erase(v.begin(), v.begin() + static_cast<ptrdiff_t>(n));
                else               v.clear();
            };
        drop(mut_rate); drop(mut_range); drop(avg_offspring);
        drop(avg_lifetime); drop(avg_cells); drop(avg_springs); drop(avg_energy);
    }

    size_t size() const { return mut_rate.size(); }
};

// ── Main history ring ─────────────────────────────────────────────────────────
struct PopulationHistory
{
    // ── Tunables — everything that shapes sampling/eviction lives here ────
    static constexpr size_t k_max_samples = 38192; // samples kept before eviction
    static constexpr size_t k_evict_chunk = 512;   // amortises erase cost to O(1)/push
    static constexpr int    k_frame_stride = 30;    // record 1 sample every N sim frames
    static constexpr int    k_spike_delta = 20;    // protozoa Δ that creates an event
    static constexpr float  k_event_cooldown_frames = 3000.f; // min frames between auto-events
    // ^ tune to taste: frames = seconds * your sim's tick rate, not wall-clock seconds.

    // `time` holds the raw simulation frame number the sample was taken at.
    // Everything in this struct is frame-indexed — there is no seconds unit
    // anywhere in here, on purpose, so callers can't accidentally mix scales.
    std::vector<float> time = {};
    std::vector<float> protozoa = {};
    std::vector<float> food = {};
    std::vector<float> total = {};
    std::vector<float> avg_gen = {};
    MiscSeries         misc = {};

    std::vector<PopulationEvent> events = {};

    // ── Mutation ─────────────────────────────────────────────────────────────

    // Call this once per simulation tick — every tick, unconditionally.
    // PopulationHistory owns the sampling cadence internally (k_frame_stride)
    // so callers never need their own throttle counter. Population and misc
    // stats are pushed together in the same call so the two series can never
    // drift out of index-alignment with each other or with `time`.
    // Returns true if a sample was actually recorded this call.
    bool push(int p, int f, float gen,
        float mr, float mrng, float ao, float al, float ac, float asp, float ae)
    {
        ++m_frame_counter_;
        if (m_frame_counter_ % k_frame_stride != 0)
            return false;

        if (time.size() >= k_max_samples)
            _evict();

        const float t = static_cast<float>(m_frame_counter_);
        const int   prev_p = protozoa.empty() ? p : static_cast<int>(protozoa.back());
        const bool  on_cooldown = !events.empty() && (t - events.back().time) < k_event_cooldown_frames;
        const int   delta = std::abs(p - prev_p);

        time.push_back(t);
        protozoa.push_back(static_cast<float>(p));
        food.push_back(static_cast<float>(f));
        total.push_back(static_cast<float>(p + f));
        avg_gen.push_back(gen);
        misc.push(mr, mrng, ao, al, ac, asp, ae);

        if (!on_cooldown && delta > k_spike_delta)
        {
            const bool  die_off = p < prev_p;
            std::string lbl = die_off
                ? "die-off (-" + std::to_string(delta) + ")"
                : "boom (+" + std::to_string(delta) + ")";
            ImVec4 col = die_off ? ImVec4{ 1.f, 0.3f, 0.3f, 1.f }
            : ImVec4{ 0.3f, 1.f, 0.5f, 1.f };
            events.push_back({ t, std::move(lbl), col });
        }
        return true;
    }

    void add_manual_event(float t, std::string label,
        ImVec4 col = ImVec4{ 1.f, 0.8f, 0.2f, 1.f })
    {
        events.push_back({ t, std::move(label), col });
    }

    // Authoritative "now", in the same frame unit `time` is stored in. Use
    // this for the live edge of the graph instead of a separately-tracked
    // counter, so the visible window can never desync from the data.
    uint64_t current_frame() const { return m_frame_counter_; }

    // Returns the [begin, begin+count) slice of the sample arrays whose
    // time falls within [x_min, x_max]. O(log n) via binary search since
    // `time` is monotonically increasing. Callers should plot only this
    // slice instead of the full history — otherwise vertex count grows
    // without bound as the sim runs longer, regardless of what's on screen.
    void window_bounds(float x_min, float x_max, size_t& begin, size_t& count) const
    {
        const auto lo = std::lower_bound(time.begin(), time.end(), x_min);
        const auto hi = std::upper_bound(time.begin(), time.end(), x_max);
        begin = static_cast<size_t>(lo - time.begin());
        count = (hi > lo) ? static_cast<size_t>(hi - lo) : 0;
    }

    // ── Band helper ───────────────────────────────────────────────────────────

    // Computes a sliding-window min/max band over `src`.
    // Caller should cache the result and only call when `size()` changes.
    static void compute_band(const std::vector<float>& src,
        std::vector<float>& lo,
        std::vector<float>& hi,
        size_t half_window = 60)
    {
        const size_t n = src.size();
        lo.resize(n);
        hi.resize(n);
        for (size_t i = 0; i < n; ++i)
        {
            const size_t s = (i >= half_window) ? i - half_window : 0;
            const size_t e = std::min(i + half_window, n - 1);
            lo[i] = *std::min_element(src.begin() + s, src.begin() + e + 1);
            hi[i] = *std::max_element(src.begin() + s, src.begin() + e + 1);
        }
    }

    // ── Export ────────────────────────────────────────────────────────────────

    void export_csv(const std::string& path) const
    {
        std::ofstream f(path);
        f << "frame,protozoa,food,total,avg_gen,"
            "mut_rate,mut_range,avg_offspring,avg_lifetime,"
            "avg_cells,avg_springs,avg_energy\n";

        const size_t misc_n = misc.size();
        for (size_t i = 0; i < time.size(); ++i)
        {
            f << time[i] << ',' << protozoa[i] << ',' << food[i] << ','
                << total[i] << ',' << avg_gen[i];
            if (i < misc_n)
                f << ',' << misc.mut_rate[i] << ',' << misc.mut_range[i]
                << ',' << misc.avg_offspring[i] << ',' << misc.avg_lifetime[i]
                << ',' << misc.avg_cells[i] << ',' << misc.avg_springs[i]
                << ',' << misc.avg_energy[i];
            f << '\n';
        }
    }

    size_t size() const { return time.size(); }

private:
    uint64_t m_frame_counter_ = 0; // raw sim ticks seen; only every k_frame_stride'th is recorded

    // Drop k_evict_chunk elements from every series.  Amortises the O(n) erase
    // cost so the per-push amortised cost is O(1).
    void _evict()
    {
        auto drop = [](std::vector<float>& v, size_t n)
            {
                if (v.size() >= n) v.erase(v.begin(), v.begin() + static_cast<ptrdiff_t>(n));
                else               v.clear();
            };
        drop(time, k_evict_chunk);
        drop(protozoa, k_evict_chunk);
        drop(food, k_evict_chunk);
        drop(total, k_evict_chunk);
        drop(avg_gen, k_evict_chunk);
        misc.evict(k_evict_chunk);

        // Prune events that now predate the earliest timestamp.
        if (!time.empty())
        {
            const float oldest = time.front();
            events.erase(std::remove_if(events.begin(), events.end(),
                [oldest](const PopulationEvent& e) { return e.time < oldest; }),
                events.end());
        }
    }
};