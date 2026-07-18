#include "ProcessingJobExecutor.h"

#include "ProcessingParameterRules.h"
#include "ProcessingStatusFormatter.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

namespace {
constexpr int kMaxOptimizationPreviewEdge = 256;
constexpr int kMaxOptimizationPreviewRadius = 64;
constexpr int kMaxOptimizationIterations = 20;

bool IsCanceled(const std::atomic_bool* cancel_requested)
{
    return cancel_requested && cancel_requested->load();
}

int OptimizationPreviewScale(const std::vector<StitchTile>& tiles)
{
    int max_edge = 1;
    for (const StitchTile& tile : tiles) {
        if (tile.frame.IsValid()) {
            max_edge = std::max(max_edge, std::max(tile.frame.width, tile.frame.height));
        }
    }
    return std::max(1, (max_edge + kMaxOptimizationPreviewEdge - 1) / kMaxOptimizationPreviewEdge);
}

ImageFrame DownsampleAverage(const ImageFrame& source, int scale)
{
    if (!source.IsValid() || scale <= 1) {
        return source;
    }

    ImageFrame preview;
    preview.width = std::max(1, (source.width + scale - 1) / scale);
    preview.height = std::max(1, (source.height + scale - 1) / scale);
    preview.stride = (preview.width * 3 + 3) & ~3;
    preview.timestamp = source.timestamp;
    preview.sequence = source.sequence;
    preview.bgr.assign(
        static_cast<std::size_t>(preview.stride) * static_cast<std::size_t>(preview.height),
        0);

    for (int y = 0; y < preview.height; ++y) {
        const int source_y0 = y * scale;
        const int source_y1 = std::min(source.height, source_y0 + scale);
        unsigned char* preview_row =
            preview.bgr.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(preview.stride);
        for (int x = 0; x < preview.width; ++x) {
            const int source_x0 = x * scale;
            const int source_x1 = std::min(source.width, source_x0 + scale);
            int blue_sum = 0;
            int green_sum = 0;
            int red_sum = 0;
            int samples = 0;
            for (int source_y = source_y0; source_y < source_y1; ++source_y) {
                const unsigned char* source_row =
                    source.bgr.data() + static_cast<std::size_t>(source_y) * static_cast<std::size_t>(source.stride);
                for (int source_x = source_x0; source_x < source_x1; ++source_x) {
                    const unsigned char* source_pixel = source_row + static_cast<std::size_t>(source_x) * 3U;
                    blue_sum += source_pixel[0];
                    green_sum += source_pixel[1];
                    red_sum += source_pixel[2];
                    ++samples;
                }
            }
            unsigned char* preview_pixel = preview_row + static_cast<std::size_t>(x) * 3U;
            preview_pixel[0] = static_cast<unsigned char>(blue_sum / std::max(1, samples));
            preview_pixel[1] = static_cast<unsigned char>(green_sum / std::max(1, samples));
            preview_pixel[2] = static_cast<unsigned char>(red_sum / std::max(1, samples));
        }
    }

    return preview;
}

std::vector<StitchTile> BuildOptimizationTiles(const std::vector<StitchTile>& tiles, int scale)
{
    if (scale <= 1) {
        return tiles;
    }

    std::vector<StitchTile> optimization_tiles;
    optimization_tiles.reserve(tiles.size());
    for (const StitchTile& tile : tiles) {
        StitchTile optimization_tile;
        optimization_tile.frame = DownsampleAverage(tile.frame, scale);
        optimization_tile.offset_x = static_cast<int>(
            std::lround(static_cast<double>(tile.offset_x) / static_cast<double>(scale)));
        optimization_tile.offset_y = static_cast<int>(
            std::lround(static_cast<double>(tile.offset_y) / static_cast<double>(scale)));
        optimization_tiles.push_back(std::move(optimization_tile));
    }
    return optimization_tiles;
}

StitchOptimizationOptions BuildPreviewOptimizationOptions(
    const std::vector<StitchTile>& optimization_tiles,
    int search_percent)
{
    StitchOptimizationOptions options =
        ProcessingParameterRules::StitchOptimizationOptionsFor(optimization_tiles, search_percent);
    options.search_radius_x = std::clamp(options.search_radius_x, 1, kMaxOptimizationPreviewRadius);
    options.search_radius_y = std::clamp(options.search_radius_y, 1, kMaxOptimizationPreviewRadius);
    options.iterations = std::clamp(options.iterations, 1, kMaxOptimizationIterations);
    return options;
}

std::vector<StitchTile> ApplyOptimizedOffsets(
    std::vector<StitchTile> full_tiles,
    const StitchOptimizationResult& optimized,
    int scale)
{
    if (!optimized.optimized || optimized.tiles.size() != full_tiles.size()) {
        return full_tiles;
    }

    const int optimized_anchor_x = optimized.tiles.empty() ? 0 : optimized.tiles[0].offset_x;
    const int optimized_anchor_y = optimized.tiles.empty() ? 0 : optimized.tiles[0].offset_y;
    const int full_anchor_x = full_tiles.empty() ? 0 : full_tiles[0].offset_x;
    const int full_anchor_y = full_tiles.empty() ? 0 : full_tiles[0].offset_y;
    for (std::size_t index = 0; index < full_tiles.size(); ++index) {
        full_tiles[index].offset_x =
            full_anchor_x + (optimized.tiles[index].offset_x - optimized_anchor_x) * scale;
        full_tiles[index].offset_y =
            full_anchor_y + (optimized.tiles[index].offset_y - optimized_anchor_y) * scale;
    }
    return full_tiles;
}
}

ProcessingJobResult ProcessingJobExecutor::RunStitch(
    unsigned long long job_id,
    std::vector<StitchTile> tiles,
    int search_percent,
    const std::atomic_bool* cancel_requested,
    const ProgressCallback& progress_callback)
{
    ProcessingJobResult result;
    result.job_id = job_id;
    result.kind = ProcessingJobKind::Stitch;

    const int optimization_scale = OptimizationPreviewScale(tiles);
    const std::vector<StitchTile> optimization_tiles =
        BuildOptimizationTiles(tiles, optimization_scale);
    const StitchOptimizationOptions optimization_options =
        BuildPreviewOptimizationOptions(optimization_tiles, search_percent);

    auto alignment_progress = [&progress_callback](int percent) {
        if (progress_callback) {
            progress_callback((percent * 30) / 100);
        }
    };
    StitchOptimizationResult optimized = ImageStitcher::OptimizeTileOffsets(
        optimization_tiles,
        optimization_options,
        cancel_requested,
        alignment_progress);
    if (IsCanceled(cancel_requested)) {
        result.status = ProcessingStatusFormatter::FormatCanceled(ProcessingJobKind::Stitch);
        return result;
    }

    std::vector<StitchTile> output_tiles =
        ApplyOptimizedOffsets(std::move(tiles), optimized, optimization_scale);
    int ready_constraint_count = optimized.constraint_count;
    const bool needs_full_resolution_refinement = optimization_scale > 1 && output_tiles.size() > 1;
    if (needs_full_resolution_refinement) {
        StitchOptimizationOptions full_refinement_options;
        full_refinement_options.search_radius_x = std::clamp(optimization_scale * 4, 6, 24);
        full_refinement_options.search_radius_y = std::clamp(optimization_scale * 4, 6, 24);
        full_refinement_options.iterations = 8;
        auto refinement_progress = [&progress_callback](int percent) {
            if (progress_callback) {
                progress_callback(30 + (percent * 10) / 100);
            }
        };
        StitchOptimizationResult full_refined = ImageStitcher::OptimizeTileOffsets(
            output_tiles,
            full_refinement_options,
            cancel_requested,
            refinement_progress);
        if (IsCanceled(cancel_requested)) {
            result.status = ProcessingStatusFormatter::FormatCanceled(ProcessingJobKind::Stitch);
            return result;
        }
        if (full_refined.optimized) {
            ready_constraint_count = std::max(ready_constraint_count, full_refined.constraint_count);
            output_tiles = std::move(full_refined.tiles);
        }
    }

    const int stitch_start_percent = needs_full_resolution_refinement ? 40 : 30;
    const int stitch_progress_span = 100 - stitch_start_percent;
    auto stitching_progress = [&progress_callback, stitch_start_percent, stitch_progress_span](int percent) {
        if (progress_callback) {
            progress_callback(stitch_start_percent + (percent * stitch_progress_span) / 100);
        }
    };
    result.image = ImageStitcher::StitchAverage(output_tiles, cancel_requested, stitching_progress);

    const bool canceled = IsCanceled(cancel_requested);
    result.succeeded = result.image.IsValid() && !canceled;
    result.status = canceled
        ? ProcessingStatusFormatter::FormatCanceled(ProcessingJobKind::Stitch)
        : result.succeeded
        ? ProcessingStatusFormatter::FormatReady(ProcessingJobKind::Stitch, result.image, ready_constraint_count)
        : ProcessingStatusFormatter::FormatFailed(ProcessingJobKind::Stitch);
    return result;
}

ProcessingJobResult ProcessingJobExecutor::RunEdf(
    unsigned long long job_id,
    std::vector<ImageFrame> stack,
    EdfOptions options,
    const std::atomic_bool* cancel_requested,
    const ProgressCallback& progress_callback)
{
    ProcessingJobResult result;
    result.job_id = job_id;
    result.kind = ProcessingJobKind::Edf;

    EdfResult edf = EdfProcessor::ComposeFocusStack(stack, options, cancel_requested, progress_callback);
    result.image = std::move(edf.composite_frame);
    result.focus_map = std::move(edf.focus_map);

    const bool canceled = IsCanceled(cancel_requested);
    result.succeeded = result.image.IsValid() && !canceled;
    result.status = canceled
        ? ProcessingStatusFormatter::FormatCanceled(ProcessingJobKind::Edf)
        : result.succeeded
        ? ProcessingStatusFormatter::FormatReady(ProcessingJobKind::Edf, result.image)
        : ProcessingStatusFormatter::FormatFailed(ProcessingJobKind::Edf);
    return result;
}
