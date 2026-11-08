#include "pheromone_grid.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <utility>

// ---- construction / resizing ---------------------------------------------------------------

PheromoneGrid::PheromoneGrid(uint32_t cells_x, uint32_t cells_y,
    float world_width, float world_height, unsigned threads)
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

void PheromoneGrid::update_cell_dimensions()
{
    cell_width = world_width / static_cast<float>(CellsX);
    cell_height = world_height / static_cast<float>(CellsY);
    inv_cell_width = static_cast<float>(CellsX) / world_width;
    inv_cell_height = static_cast<float>(CellsY) / world_height;
    max_cx = static_cast<float>(CellsX - 1);
    max_cy = static_cast<float>(CellsY - 1);
}

void PheromoneGrid::change_cell_dimensions(uint32_t new_cells_x, uint32_t new_cells_y)
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

void PheromoneGrid::clear()
{
    std::memset(front.data(), 0, front.size() * sizeof(float));
    std::memset(back.data(), 0, back.size() * sizeof(float));
    ++version_;
}

// ---- simulation step -----------------------------------------------------------------------

void PheromoneGrid::step()
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

// ---- setup ---------------------------------------------------------------------------------

void PheromoneGrid::allocate()
{
    const size_t padded = static_cast<size_t>(PaddedW) * PaddedH;
    front.assign(padded, 0.f);
    back.assign(padded, 0.f);

    // 3 rolling rows per band, padded so two bands never share a cache line
    scratch_stride_ = static_cast<size_t>(3) * CellsX + 32;
    scratch_.assign(scratch_stride_ * thread_count_, 0.f);
}

void PheromoneGrid::build_lut()
{
    // Colour ramp sampled at 256 levels of pheromone / max_pheromone.
    for (int i = 0; i < 256; ++i)
    {
        const float t = static_cast<float>(i) / 255.f;
        const float g = 0.35f + t * 0.65f;

        // Level 0 means "no pheromone": fully transparent, otherwise every empty cell
        // is drawn as a faint green square over the whole world. RGB is kept so bilinear
        // smoothing doesn't blend trail edges toward black.
        const uint8_t a = (i == 0) ? 0 : static_cast<uint8_t>((0.15f + t * 0.6f) * 255.f);

        const uint8_t rgba[4] = { 0, static_cast<uint8_t>(g * 255.f), 0, a };
        std::memcpy(&lut_[i], rgba, 4); // byte order matches the old px[0..3] writes
    }
}

void PheromoneGrid::build_jobs()
{
    if (thread_count_ <= 1)
        return;

    pool_ = std::make_unique<BarrierThreadPool>(static_cast<int>(thread_count_));
    jobs_.reserve(thread_count_);
    for (uint32_t t = 0; t < thread_count_; ++t)
        jobs_.emplace_back([this, t] { run_band(t); });
    pool_->set_jobs(jobs_); // built once; the jobs read task_/active_bands_ each run
}

// ---- dispatch ------------------------------------------------------------------------------

void PheromoneGrid::run(Task task)
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

void PheromoneGrid::run_band(uint32_t t)
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