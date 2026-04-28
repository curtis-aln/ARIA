#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <numeric>
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
    // Capacity before eviction triggers.  Eviction drops k_evict_chunk at once
    // so the amortised cost per push is O(1) rather than O(n).
    static constexpr size_t k_max_samples = 38192;
    static constexpr size_t k_evict_chunk = 512;
    static constexpr int    k_spike_delta = 20;   // protozoa Δ that creates an event
    static constexpr float  k_event_cooldown = 50.f; // minimum seconds between auto-events

    std::vector<float> time = {};
    std::vector<float> protozoa = {};
    std::vector<float> food = {};
    std::vector<float> total = {};
    std::vector<float> avg_gen = {};
    MiscSeries         misc = {};

    std::vector<PopulationEvent> events = {};

    // ── Mutation ─────────────────────────────────────────────────────────────

    void push(float t, int p, int f, float gen)
    {
        if (time.size() >= k_max_samples)
            _evict();

        const int  prev_p = protozoa.empty() ? p : static_cast<int>(protozoa.back());
        const bool on_cooldown = !events.empty() && (t - events.back().time) < k_event_cooldown;
        const int  delta = std::abs(p - prev_p);

        time.push_back(t);
        protozoa.push_back(static_cast<float>(p));
        food.push_back(static_cast<float>(f));
        total.push_back(static_cast<float>(p + f));
        avg_gen.push_back(gen);

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
    }

    void push_misc(float mr, float mrng, float ao, float al, float ac, float asp, float ae)
    {
        misc.push(mr, mrng, ao, al, ac, asp, ae);
    }

    void add_manual_event(float t, std::string label,
        ImVec4 col = ImVec4{ 1.f, 0.8f, 0.2f, 1.f })
    {
        events.push_back({ t, std::move(label), col });
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
        f << "time,protozoa,food,total,avg_gen,"
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