#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>
#include <Imgui.h>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include "../thread_pool.h"

struct PheromoneGridSettings
{
    inline static float decay_rate = 0.0015f;  // fraction of pheromone lost per step
    inline static float diffuse_rate = 0.20f;  // blend toward weighted neighbour average per step
    inline static float deposit_amount = 10.0f;  // default amount added per add_pheromone call
    inline static float max_pheromone = 100.0f; // clamp ceiling, also used to normalise heatmap colour
    inline static uint32_t substeps = 2;      // diffusion sub-iterations per step() call
    inline static uint32_t parallel_min_cells = 1u << 16; // below this many cells, step/bake run on the calling thread
};

// Dense full-grid diffusion on a row-major grid with a 1-cell clamp-to-edge border.
//
// What changed versus the previous version:
//  * step() is a single fused sweep: each output row needs the horizontally blurred rows
//    above/mid/below it, and those are kept in a 3-row rolling buffer that lives in L1/L2,
//    instead of writing a whole-grid h_pass array and reading it back. Main-memory traffic
//    per substep drops from ~5 grid passes to ~2 (read front, write back).
//  * The sweep is split into independent row bands that run on a BarrierThreadPool. Each band
//    recomputes its own two halo rows, so there is only one barrier per substep.
//  * The float->RGBA heatmap conversion is a 256-entry lookup table with one 32-bit store per
//    cell, runs in parallel, and only runs when step() has actually changed the field.
//  * get_texture()/get_sprite() return const references. Returning sf::Texture by value copied
//    the whole GPU texture on every call.
//  * track_stats() caches its result and only rescans the grid after a step()/clear().
//  * hash() clamps in float space before casting (a negative or NaN float -> uint32_t cast is UB).
//
// Not copyable or movable: the thread-pool jobs capture `this` and the sprite points at the texture.
class PheromoneGrid : PheromoneGridSettings
{
public:
    explicit PheromoneGrid(uint32_t cells_x, uint32_t cells_y,
        float world_width, float world_height, unsigned threads = 1)
        : CellsX(cells_x), CellsY(cells_y)
        , PaddedW(cells_x + 2), PaddedH(cells_y + 2)
        , world_width(world_width), world_height(world_height)
        , thread_count_(std::max(1u, threads))
    {
        assert(cells_x > 0 && cells_y > 0);

        build_lut();
        allocate();
        update_cell_dimensions();
        resize_render_texture();
        build_jobs();
    }

    PheromoneGrid(const PheromoneGrid&) = delete;
    PheromoneGrid& operator=(const PheromoneGrid&) = delete;
    PheromoneGrid(PheromoneGrid&&) = delete;
    PheromoneGrid& operator=(PheromoneGrid&&) = delete;

    void update_cell_dimensions()
    {
        cell_width = world_width / static_cast<float>(CellsX);
        cell_height = world_height / static_cast<float>(CellsY);
        inv_cell_width = static_cast<float>(CellsX) / world_width;
        inv_cell_height = static_cast<float>(CellsY) / world_height;
        max_cx = static_cast<float>(CellsX - 1);
        max_cy = static_cast<float>(CellsY - 1);
    }

    void change_cell_dimensions(uint32_t new_cells_x, uint32_t new_cells_y)
    {
        assert(new_cells_x > 0 && new_cells_y > 0);
        CellsX = new_cells_x;
        CellsY = new_cells_y;
        PaddedW = CellsX + 2;
        PaddedH = CellsY + 2;
        update_cell_dimensions();
        allocate();
        resize_render_texture();
        ++version_;
    }

    uint32_t hash(const float x, const float y) const
    {
        // Clamp before the cast: float -> uint32_t is UB for negative, NaN or huge values.
        // std::max(0.f, v) returns 0 for NaN, which is what we want.
        const float fx = std::min(std::max(0.f, x * inv_cell_width), max_cx);
        const float fy = std::min(std::max(0.f, y * inv_cell_height), max_cy);
        return (static_cast<uint32_t>(fy) + 1) * PaddedW + (static_cast<uint32_t>(fx) + 1);
    }

    void add_pheromone(const float x, const float y, float amount = -1.f)
    {
        add_pheromone_at_index(hash(x, y), amount);
    }

    void add_pheromone_at_index(const uint32_t index, float amount = -1.f)
    {
        if (amount < 0.f) amount = deposit_amount;
        float& v = front[index];
        v = std::min(v + amount, max_pheromone);
    }

    float sample(const float x, const float y) const { return front[hash(x, y)]; }
    float sample_at_index(const uint32_t index) const { return front[index]; }

    void clear()
    {
        std::memset(front.data(), 0, front.size() * sizeof(float));
        std::memset(back.data(), 0, back.size() * sizeof(float));
        ++version_;
    }

    // Diffuses with a separable 1-2-1 kernel, then decays. Call once per update interval.
    // Deposits made since the last call are included. Increments version().
    void step()
    {
        const uint32_t n = std::max(1u, substeps);
        step_diffuse_ = diffuse_rate / static_cast<float>(n);
        step_keep_ = 1.f - decay_rate / static_cast<float>(n);

        for (uint32_t s = 0; s < n; ++s)
        {
            replicate_border();
            run(Task::Diffuse);
            std::swap(front, back);
        }
        ++version_;
    }

    // ---- Rendering -------------------------------------------------------------------------
    // Bumped by step() and clear(). Compare against what you last rendered instead of using a
    // modulo on the iteration counter (which can alias with the sub-iteration multiplier).
    uint64_t version() const { return version_; }
    bool needs_render() const { return baked_version_ != version_; }

    // CPU only, no GL calls: safe to run on the simulation thread. Fills pixel_data() (RGBA8).
    void bake_pixels()
    {
        bake_scale_ = max_pheromone > 0.f ? 255.f / max_pheromone : 0.f;
        run(Task::Bake);
        baked_version_ = version_;
    }

    // GL call: must run on the thread that owns the GL context used for drawing.
    void upload_texture()
    {
        heat_texture.update(reinterpret_cast<const uint8_t*>(pixel_buffer.data()));
    }

    // Convenience: bake + upload only if the field changed, then optionally draw.
    void render(sf::RenderWindow* window = nullptr)
    {
        if (needs_render())
        {
            bake_pixels();
            upload_texture();
        }
        if (window != nullptr)
            window->draw(heat_sprite);
    }

    const uint8_t* pixel_data() const { return reinterpret_cast<const uint8_t*>(pixel_buffer.data()); }

    void track_stats()
    {
        if (stats_version_ != version_)
            compute_stats();

        const float tc = static_cast<float>(get_total_cells());

        ImGui::Spacing();
        ImGui::Text("Total     %.1f", stat_total_);
        ImGui::Text("Avg/cell  %.3f", tc > 0.f ? stat_total_ / tc : 0.f);
        ImGui::Text("Max cell  %.2f  (%.0f%%)", stat_max_,
            max_pheromone > 0.f ? stat_max_ * 100.f / max_pheromone : 0.f);
        ImGui::Text("Active    %d  (%.1f%%)", stat_active_, tc > 0.f ? stat_active_ * 100.f / tc : 0.f);
    }

    size_t get_total_cells() const { return static_cast<size_t>(CellsX) * CellsY; }

    const sf::Texture& get_texture() const { return heat_texture; }
    const sf::Sprite& get_sprite() const { return heat_sprite; }
    sf::Vector2i get_cell_dimensions() const { return { static_cast<int>(CellsX), static_cast<int>(CellsY) }; }
    sf::Vector2f get_grid_size() const { return { cell_width, cell_height }; }

private:
    enum class Task : uint8_t { Diffuse, Bake };

    // ---- setup -----------------------------------------------------------------------------
    void allocate()
    {
        const size_t padded = static_cast<size_t>(PaddedW) * PaddedH;
        front.assign(padded, 0.f);
        back.assign(padded, 0.f);

        // 3 rolling rows per band, padded so two bands never share a cache line
        scratch_stride_ = static_cast<size_t>(3) * CellsX + 32;
        scratch_.assign(scratch_stride_ * thread_count_, 0.f);
    }

    void build_lut()
    {
        // Same colour ramp as before, sampled at 256 levels of pheromone / max_pheromone.
        for (int i = 0; i < 256; ++i)
        {
            const float t = static_cast<float>(i) / 255.f;
            const float g = 0.35f + t * 0.65f;
            const uint8_t rgba[4] = {
                0,
                static_cast<uint8_t>(g * 255.f),
                0,
                static_cast<uint8_t>((0.15f + t * 0.6f) * 255.f) };
            std::memcpy(&lut_[i], rgba, 4); // byte order matches the old px[0..3] writes
        }
    }

    void build_jobs()
    {
        if (thread_count_ <= 1)
            return;

        pool_ = std::make_unique<BarrierThreadPool>(static_cast<int>(thread_count_));
        jobs_.reserve(thread_count_);
        for (uint32_t t = 0; t < thread_count_; ++t)
            jobs_.emplace_back([this, t] { run_band(t); });
        pool_->set_jobs(jobs_); // built once; the jobs read task_/active_bands_ each run
    }

    // ---- dispatch --------------------------------------------------------------------------
    void run(Task task)
    {
        task_ = task;
        if (pool_ && get_total_cells() >= parallel_min_cells)
        {
            active_bands_ = thread_count_;
            pool_->run_and_wait();
        }
        else
        {
            active_bands_ = 1;
            run_band(0);
        }
    }

    void run_band(uint32_t t)
    {
        const uint32_t rows_per = (CellsY + active_bands_ - 1) / active_bands_;
        const uint32_t y0 = t * rows_per;
        if (y0 >= CellsY)
            return;
        const uint32_t y1 = std::min(y0 + rows_per, CellsY);

        switch (task_)
        {
        case Task::Diffuse: diffuse_rows(y0, y1, scratch_.data() + t * scratch_stride_); break;
        case Task::Bake:    bake_rows(y0, y1); break;
        }
    }

    // ---- kernels ---------------------------------------------------------------------------
    static inline void hblur_row(const float* __restrict row, float* __restrict out, const uint32_t n)
    {
        for (uint32_t x = 0; x < n; ++x)
            out[x] = (row[x] + 2.f * row[x + 1] + row[x + 2]) * 0.25f;
    }

    static inline void vertical_blend(const float* __restrict top, const float* __restrict mid,
        const float* __restrict bot, const float* __restrict orig, float* __restrict out,
        const uint32_t n, const float diffuse, const float keep)
    {
        for (uint32_t x = 0; x < n; ++x)
        {
            const float avg = (top[x] + 2.f * mid[x] + bot[x]) * 0.25f;
            float v = orig[x] + (avg - orig[x]) * diffuse;
            v *= keep;
            out[x] = v < 0.01f ? 0.f : v;
        }
    }

    // Output rows [y0, y1) (interior coordinates). Output row y needs the horizontally blurred
    // padded rows y, y+1, y+2, so each band primes two rows and then rolls down.
    void diffuse_rows(const uint32_t y0, const uint32_t y1, float* scratch)
    {
        const float* src = front.data();
        float* dst = back.data();

        float* top = scratch;
        float* mid = scratch + CellsX;
        float* bot = scratch + 2 * static_cast<size_t>(CellsX);

        hblur_row(src + static_cast<size_t>(y0) * PaddedW, top, CellsX);
        hblur_row(src + static_cast<size_t>(y0 + 1) * PaddedW, mid, CellsX);

        for (uint32_t y = y0; y < y1; ++y)
        {
            hblur_row(src + static_cast<size_t>(y + 2) * PaddedW, bot, CellsX);

            vertical_blend(top, mid, bot,
                src + static_cast<size_t>(y + 1) * PaddedW + 1,
                dst + static_cast<size_t>(y + 1) * PaddedW + 1,
                CellsX, step_diffuse_, step_keep_);

            float* recycled = top;
            top = mid;
            mid = bot;
            bot = recycled;
        }
    }

    void bake_rows(const uint32_t y0, const uint32_t y1)
    {
        const float scale = bake_scale_;
        const uint32_t* __restrict lut = lut_.data();

        for (uint32_t y = y0; y < y1; ++y)
        {
            const float* __restrict row = &front[static_cast<size_t>(y + 1) * PaddedW + 1];
            uint32_t* __restrict out = &pixel_buffer[static_cast<size_t>(y) * CellsX];

            for (uint32_t x = 0; x < CellsX; ++x)
                out[x] = lut[static_cast<uint32_t>(std::min(row[x] * scale + 0.5f, 255.f))];
        }
    }

    // Replicates edge cells into the 1-cell border so the kernel never bounds-checks.
    void replicate_border()
    {
        for (uint32_t y = 1; y <= CellsY; ++y)
        {
            front[y * PaddedW] = front[y * PaddedW + 1];
            front[y * PaddedW + PaddedW - 1] = front[y * PaddedW + CellsX];
        }
        for (uint32_t x = 0; x < PaddedW; ++x)
        {
            front[x] = front[PaddedW + x];
            front[(PaddedH - 1) * PaddedW + x] = front[CellsY * PaddedW + x];
        }
    }

    void compute_stats()
    {
        float total = 0.f, max_v = 0.f;
        int active = 0;

        for (uint32_t cy = 0; cy < CellsY; ++cy)
        {
            const float* row = &front[static_cast<size_t>(cy + 1) * PaddedW + 1];
            for (uint32_t cx = 0; cx < CellsX; ++cx)
            {
                const float v = row[cx];
                total += v;
                if (v > max_v) max_v = v;
                if (v > 0.f)   ++active;
            }
        }

        stat_total_ = total;
        stat_max_ = max_v;
        stat_active_ = active;
        stats_version_ = version_;
    }

    void resize_render_texture()
    {
        // SFML 3 renamed Texture::create to Texture::resize
        if (!heat_texture.resize({ CellsX, CellsY }))
            throw std::runtime_error("Failed to resize pheromone heatmap texture.");

        heat_texture.setSmooth(true);
        heat_sprite.setTexture(heat_texture, true);
        heat_sprite.setScale({ cell_width, cell_height });
        heat_sprite.setPosition({ 0.f, 0.f });

        pixel_buffer.assign(static_cast<size_t>(CellsX) * CellsY, 0u);
        baked_version_ = 0; // force a bake on the next render()
    }

    // ---- data ------------------------------------------------------------------------------
    uint32_t CellsX = 0;
    uint32_t CellsY = 0;
    uint32_t PaddedW = 0;
    uint32_t PaddedH = 0;

    float cell_width = 0;
    float cell_height = 0;
    float world_width = 0;
    float world_height = 0;
    float inv_cell_width = 0;
    float inv_cell_height = 0;
    float max_cx = 0;
    float max_cy = 0;

    uint32_t thread_count_ = 1;

    std::vector<float> front{};
    std::vector<float> back{};
    std::vector<float> scratch_{};   // per-band rolling rows for the fused diffusion pass
    size_t scratch_stride_ = 0;

    sf::Texture heat_texture{};
    sf::Sprite  heat_sprite{ heat_texture };
    std::vector<uint32_t> pixel_buffer{}; // one RGBA8 pixel per cell
    std::array<uint32_t, 256> lut_{};

    // per-run parameters read by the pool jobs
    Task     task_ = Task::Diffuse;
    uint32_t active_bands_ = 1;
    float    step_diffuse_ = 0.f;
    float    step_keep_ = 1.f;
    float    bake_scale_ = 0.f;

    uint64_t version_ = 1;
    uint64_t baked_version_ = 0;

    uint64_t stats_version_ = 0;
    float stat_total_ = 0.f;
    float stat_max_ = 0.f;
    int   stat_active_ = 0;

    // declared last so the pool (and its worker threads) is destroyed before the data it uses
    std::vector<std::function<void()>> jobs_{};
    std::unique_ptr<BarrierThreadPool> pool_{};
};