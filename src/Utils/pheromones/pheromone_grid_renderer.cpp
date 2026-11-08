#include "pheromone_grid.h"

#include <stdexcept>
#include <Imgui.h>
#include <SFML/Graphics/RenderWindow.hpp>

void PheromoneGrid::bake_pixels()
{
    bake_scale_ = max_pheromone > 0.f ? 255.f / max_pheromone : 0.f;
    run(Task::Bake);
    baked_version_ = version_;
}

void PheromoneGrid::upload_texture()
{
    heat_texture.update(reinterpret_cast<const uint8_t*>(pixel_buffer.data()));
}

void PheromoneGrid::render(sf::RenderWindow* window)
{
    if (needs_render())
    {
        bake_pixels();
        upload_texture();
    }
    if (window != nullptr)
        window->draw(heat_sprite);
}

void PheromoneGrid::resize_render_texture()
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

void PheromoneGrid::track_stats()
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

void PheromoneGrid::compute_stats()
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