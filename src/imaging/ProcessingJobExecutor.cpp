#include "ProcessingJobExecutor.h"

#include "ProcessingParameterRules.h"
#include "ProcessingStatusFormatter.h"

#include <utility>

namespace {
bool IsCanceled(const std::atomic_bool* cancel_requested)
{
    return cancel_requested && cancel_requested->load();
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

    const StitchOptimizationOptions optimization_options =
        ProcessingParameterRules::StitchOptimizationOptionsFor(tiles, search_percent);

    auto alignment_progress = [&progress_callback](int percent) {
        if (progress_callback) {
            progress_callback((percent * 30) / 100);
        }
    };
    StitchOptimizationResult optimized = ImageStitcher::OptimizeTileOffsets(
        tiles,
        optimization_options,
        cancel_requested,
        alignment_progress);
    if (IsCanceled(cancel_requested)) {
        result.status = ProcessingStatusFormatter::FormatCanceled(ProcessingJobKind::Stitch);
        return result;
    }

    auto stitching_progress = [&progress_callback](int percent) {
        if (progress_callback) {
            progress_callback(30 + (percent * 70) / 100);
        }
    };
    const std::vector<StitchTile>& output_tiles = optimized.optimized ? optimized.tiles : tiles;
    result.image = ImageStitcher::StitchAverage(output_tiles, cancel_requested, stitching_progress);

    const bool canceled = IsCanceled(cancel_requested);
    result.succeeded = result.image.IsValid() && !canceled;
    result.status = canceled
        ? ProcessingStatusFormatter::FormatCanceled(ProcessingJobKind::Stitch)
        : result.succeeded
        ? ProcessingStatusFormatter::FormatReady(ProcessingJobKind::Stitch, result.image, optimized.constraint_count)
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
