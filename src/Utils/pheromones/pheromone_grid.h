#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>
#include "../thread_pool.h"

namespace sf { class RenderWindow; }

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
//
// File layout:
//   pheromone_grid.h              - this declaration + hot-path inline accessors
//   pheromone_grid.cpp            - construction, resizing, clear, step(), thread-pool dispatch
//   pheromone_grid_kernels.cpp    - diffusion / bake / border kernels (the hot loops)
//   pheromone_grid_render.cpp     - texture upload, drawing, ImGui stats
class PheromoneGrid : PheromoneGridSettings
{
public:
    explicit PheromoneGrid(uint32_t cells_x, uint32_t cells_y,
        float world_width, float world_height, unsigned threads = 1);

    PheromoneGrid(const PheromoneGrid&) = delete;
    PheromoneGrid& operator=(const PheromoneGrid&) = delete;
    PheromoneGrid(PheromoneGrid&&) = delete;
    PheromoneGrid& operator=(PheromoneGrid&&) = delete;

    void update_cell_dimensions();
    void change_cell_dimensions(uint32_t new_cells_x, uint32_t new_cells_y);

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

    void clear();

    // Diffuses with a separable 1-2-1 kernel, then decays. Call once per update interval.
    // Deposits made since the last call are included. Increments version().
    void step();

    // ---- Rendering -------------------------------------------------------------------------
    // Bumped by step() and clear(). Compare against what you last rendered instead of using a
    // modulo on the iteration counter (which can alias with the sub-iteration multiplier).
    uint64_t version() const { return version_; }
    bool needs_render() const { return baked_version_ != version_; }

    // CPU only, no GL calls: safe to run on the simulation thread. Fills pixel_data() (RGBA8).
    void bake_pixels();

    // GL call: must run on the thread that owns the GL context used for drawing.
    void upload_texture();

    // Convenience: bake + upload only if the field changed, then optionally draw.
    void render(sf::RenderWindow* window = nullptr);

    const uint8_t* pixel_data() const { return reinterpret_cast<const uint8_t*>(pixel_buffer.data()); }

    void track_stats();

    size_t get_total_cells() const { return static_cast<size_t>(CellsX) * CellsY; }

    const sf::Texture& get_texture() const { return heat_texture; }
    const sf::Sprite& get_sprite() const { return heat_sprite; }
    sf::Vector2i get_cell_dimensions() const { return { static_cast<int>(CellsX), static_cast<int>(CellsY) }; }
    sf::Vector2f get_grid_size() const { return { cell_width, cell_height }; }

private:
    enum class Task : uint8_t { Diffuse, Bake };

    // setup (pheromone_grid.cpp)
    void allocate();
    void build_lut();
    void build_jobs();

    // dispatch (pheromone_grid.cpp)
    void run(Task task);
    void run_band(uint32_t t);

    // kernels (pheromone_grid_kernels.cpp)
    void diffuse_rows(uint32_t y0, uint32_t y1, float* scratch);
    void bake_rows(uint32_t y0, uint32_t y1);
    void replicate_border();

    // rendering / stats (pheromone_grid_render.cpp)
    void compute_stats();
    void resize_render_texture();

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