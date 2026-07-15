#include "ImageStitcher.h"

#include "ImageRegistration.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace {

struct Bounds {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

struct TileConstraint {
    std::size_t reference_index = 0;
    std::size_t moving_index = 0;
    int dx = 0;
    int dy = 0;
    double weight = 1.0;
};

bool ExpandBounds(const StitchTile& tile, Bounds& bounds, bool first)
{
    if (!tile.frame.IsValid()) {
        return false;
    }

    const int left = tile.offset_x;
    const int top = tile.offset_y;
    const int right = tile.offset_x + tile.frame.width;
    const int bottom = tile.offset_y + tile.frame.height;
    if (first) {
        bounds = Bounds{left, top, right, bottom};
    } else {
        bounds.left = std::min(bounds.left, left);
        bounds.top = std::min(bounds.top, top);
        bounds.right = std::max(bounds.right, right);
        bounds.bottom = std::max(bounds.bottom, bottom);
    }
    return true;
}

bool IsCancelled(const std::atomic_bool* cancel_requested)
{
    return cancel_requested && cancel_requested->load();
}

void ReportProgress(const std::function<void(int)>& progress_callback, int percent)
{
    if (progress_callback) {
        progress_callback(std::clamp(percent, 0, 100));
    }
}

} // namespace

StitchOptimizationResult ImageStitcher::OptimizeTileOffsets(
    const std::vector<StitchTile>& tiles,
    StitchOptimizationOptions options,
    const std::atomic_bool* cancel_requested,
    const std::function<void(int)>& progress_callback)
{
    StitchOptimizationResult result;
    result.tiles = tiles;
    ReportProgress(progress_callback, 0);
    if (tiles.size() < 2 || IsCancelled(cancel_requested)) {
        return result;
    }

    options.search_radius_x = std::max(0, options.search_radius_x);
    options.search_radius_y = std::max(0, options.search_radius_y);
    options.iterations = std::max(1, options.iterations);

    const int pair_count = static_cast<int>((tiles.size() * (tiles.size() - 1U)) / 2U);
    int processed_pairs = 0;
    std::vector<TileConstraint> constraints;
    constraints.reserve(static_cast<std::size_t>(pair_count));

    for (std::size_t reference_index = 0; reference_index < tiles.size(); ++reference_index) {
        if (!tiles[reference_index].frame.IsValid()) {
            continue;
        }
        for (std::size_t moving_index = reference_index + 1U; moving_index < tiles.size(); ++moving_index) {
            if (IsCancelled(cancel_requested)) {
                return result;
            }
            ++processed_pairs;
            if (!tiles[moving_index].frame.IsValid()) {
                ReportProgress(progress_callback, (processed_pairs * 70) / std::max(1, pair_count));
                continue;
            }

            const int initial_dx = tiles[moving_index].offset_x - tiles[reference_index].offset_x;
            const int initial_dy = tiles[moving_index].offset_y - tiles[reference_index].offset_y;
            const TranslationOffset refined = ImageRegistration::RefineTranslation(
                tiles[reference_index].frame,
                tiles[moving_index].frame,
                initial_dx,
                initial_dy,
                options.search_radius_x,
                options.search_radius_y);
            if (refined.valid) {
                TileConstraint constraint;
                constraint.reference_index = reference_index;
                constraint.moving_index = moving_index;
                constraint.dx = refined.dx;
                constraint.dy = refined.dy;
                constraint.weight = 1.0 / (1.0 + std::max(0.0, refined.score));
                constraints.push_back(constraint);
            }
            ReportProgress(progress_callback, (processed_pairs * 70) / std::max(1, pair_count));
        }
    }

    result.constraint_count = static_cast<int>(constraints.size());
    if (constraints.empty() || IsCancelled(cancel_requested)) {
        ReportProgress(progress_callback, 100);
        return result;
    }

    std::vector<double> x(tiles.size(), 0.0);
    std::vector<double> y(tiles.size(), 0.0);
    std::vector<double> original_x(tiles.size(), 0.0);
    std::vector<double> original_y(tiles.size(), 0.0);
    for (std::size_t index = 0; index < tiles.size(); ++index) {
        x[index] = original_x[index] = static_cast<double>(tiles[index].offset_x);
        y[index] = original_y[index] = static_cast<double>(tiles[index].offset_y);
    }

    constexpr double kOriginalOffsetWeight = 0.05;
    for (int iteration = 0; iteration < options.iterations; ++iteration) {
        if (IsCancelled(cancel_requested)) {
            return result;
        }

        std::vector<double> next_x = x;
        std::vector<double> next_y = y;
        for (std::size_t tile_index = 1; tile_index < tiles.size(); ++tile_index) {
            double sum_x = original_x[tile_index] * kOriginalOffsetWeight;
            double sum_y = original_y[tile_index] * kOriginalOffsetWeight;
            double sum_weight = kOriginalOffsetWeight;

            for (const TileConstraint& constraint : constraints) {
                if (constraint.reference_index == tile_index) {
                    sum_x += (x[constraint.moving_index] - constraint.dx) * constraint.weight;
                    sum_y += (y[constraint.moving_index] - constraint.dy) * constraint.weight;
                    sum_weight += constraint.weight;
                } else if (constraint.moving_index == tile_index) {
                    sum_x += (x[constraint.reference_index] + constraint.dx) * constraint.weight;
                    sum_y += (y[constraint.reference_index] + constraint.dy) * constraint.weight;
                    sum_weight += constraint.weight;
                }
            }

            if (sum_weight > 0.0) {
                next_x[tile_index] = sum_x / sum_weight;
                next_y[tile_index] = sum_y / sum_weight;
            }
        }

        x = std::move(next_x);
        y = std::move(next_y);
        x[0] = original_x[0];
        y[0] = original_y[0];
        ReportProgress(
            progress_callback,
            70 + ((iteration + 1) * 30) / options.iterations);
    }

    for (std::size_t index = 0; index < result.tiles.size(); ++index) {
        result.tiles[index].offset_x = static_cast<int>(std::lround(x[index]));
        result.tiles[index].offset_y = static_cast<int>(std::lround(y[index]));
    }
    result.optimized = true;
    ReportProgress(progress_callback, 100);
    return result;
}

ImageFrame ImageStitcher::StitchAverage(
    const std::vector<StitchTile>& tiles,
    const std::atomic_bool* cancel_requested,
    const std::function<void(int)>& progress_callback)
{
    ReportProgress(progress_callback, 0);
    if (IsCancelled(cancel_requested)) {
        return {};
    }

    Bounds bounds;
    bool has_tile = false;
    for (const StitchTile& tile : tiles) {
        if (IsCancelled(cancel_requested)) {
            return {};
        }
        if (ExpandBounds(tile, bounds, !has_tile)) {
            has_tile = true;
        }
    }
    if (!has_tile || bounds.right <= bounds.left || bounds.bottom <= bounds.top) {
        return {};
    }
    ReportProgress(progress_callback, 5);

    ImageFrame output;
    output.width = bounds.right - bounds.left;
    output.height = bounds.bottom - bounds.top;
    output.stride = (output.width * 3 + 3) & ~3;
    output.bgr.assign(static_cast<std::size_t>(output.stride) * static_cast<std::size_t>(output.height), 0);

    const std::size_t pixel_count = static_cast<std::size_t>(output.width) * static_cast<std::size_t>(output.height);
    std::vector<unsigned int> red(pixel_count, 0);
    std::vector<unsigned int> green(pixel_count, 0);
    std::vector<unsigned int> blue(pixel_count, 0);
    std::vector<unsigned int> count(pixel_count, 0);

    int accumulation_rows = 0;
    for (const StitchTile& tile : tiles) {
        if (tile.frame.IsValid()) {
            accumulation_rows += tile.frame.height;
        }
    }
    int processed_rows = 0;
    for (const StitchTile& tile : tiles) {
        if (IsCancelled(cancel_requested)) {
            return {};
        }
        if (!tile.frame.IsValid()) {
            continue;
        }

        const int dest_x0 = tile.offset_x - bounds.left;
        const int dest_y0 = tile.offset_y - bounds.top;
        for (int y = 0; y < tile.frame.height; ++y) {
            if (IsCancelled(cancel_requested)) {
                return {};
            }
            const unsigned char* src = tile.frame.bgr.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(tile.frame.stride);
            for (int x = 0; x < tile.frame.width; ++x) {
                const int dest_x = dest_x0 + x;
                const int dest_y = dest_y0 + y;
                if (dest_x < 0 || dest_y < 0 || dest_x >= output.width || dest_y >= output.height) {
                    continue;
                }
                const std::size_t index = static_cast<std::size_t>(dest_y) * static_cast<std::size_t>(output.width) + static_cast<std::size_t>(dest_x);
                blue[index] += src[x * 3 + 0];
                green[index] += src[x * 3 + 1];
                red[index] += src[x * 3 + 2];
                ++count[index];
            }
            ++processed_rows;
            if (accumulation_rows > 0) {
                ReportProgress(progress_callback, 5 + (processed_rows * 70) / accumulation_rows);
            }
        }
    }

    for (int y = 0; y < output.height; ++y) {
        if (IsCancelled(cancel_requested)) {
            return {};
        }
        unsigned char* dst = output.bgr.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(output.stride);
        for (int x = 0; x < output.width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(output.width) + static_cast<std::size_t>(x);
            if (count[index] == 0) {
                continue;
            }
            dst[x * 3 + 0] = static_cast<unsigned char>(blue[index] / count[index]);
            dst[x * 3 + 1] = static_cast<unsigned char>(green[index] / count[index]);
            dst[x * 3 + 2] = static_cast<unsigned char>(red[index] / count[index]);
        }
        ReportProgress(progress_callback, 75 + ((y + 1) * 25) / output.height);
    }

    ReportProgress(progress_callback, 100);
    return output;
}
