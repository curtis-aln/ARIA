#include "pheromone_grid.h"

#include <algorithm>

namespace
{
    inline void hblur_row(const float* __restrict row, float* __restrict out, const uint32_t n)
    {
        for (uint32_t x = 0; x < n; ++x)
            out[x] = (row[x] + 2.f * row[x + 1] + row[x + 2]) * 0.25f;
    }

    inline void vertical_blend(const float* __restrict top, const float* __restrict mid,
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
}

// Output rows [y0, y1) (interior coordinates). Output row y needs the horizontally blurred
// padded rows y, y+1, y+2, so each band primes two rows and then rolls down.
void PheromoneGrid::diffuse_rows(const uint32_t y0, const uint32_t y1, float* scratch)
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

void PheromoneGrid::bake_rows(const uint32_t y0, const uint32_t y1)
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
void PheromoneGrid::replicate_border()
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