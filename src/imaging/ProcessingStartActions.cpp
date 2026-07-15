#include "ProcessingStartActions.h"

#include "ProcessingStatusFormatter.h"

ProcessingStartActionResult ProcessingStartActions::StartStitch(
    ProcessingJobState& state,
    ProcessingRetryState& retry,
    const std::vector<StitchTile>& tiles,
    int search_percent,
    bool remember_snapshot,
    const BeforeBeginCallback& before_begin)
{
    ProcessingStartActionResult result;
    result.kind = ProcessingJobKind::Stitch;
    if (tiles.empty()) {
        result.status = ProcessingStartActionStatus::NoStitchTiles;
        result.message = L"Add stitch tiles before building a stitched image.";
        return result;
    }
    if (state.IsRunning()) {
        result.status = ProcessingStartActionStatus::AlreadyRunning;
        result.message = ProcessingStatusFormatter::FormatAlreadyRunning();
        return result;
    }

    if (before_begin) {
        before_begin();
    }
    state.ClearPending();
    if (remember_snapshot) {
        retry.RememberStitch(tiles, search_percent);
    }

    result.launch = state.Begin();
    result.status = ProcessingStartActionStatus::Started;
    result.can_start = true;
    result.message = ProcessingStatusFormatter::FormatStarted(result.kind);
    return result;
}

ProcessingStartActionResult ProcessingStartActions::StartEdf(
    ProcessingJobState& state,
    ProcessingRetryState& retry,
    const std::vector<ImageFrame>& stack,
    EdfOptions options,
    bool remember_snapshot,
    const BeforeBeginCallback& before_begin)
{
    ProcessingStartActionResult result;
    result.kind = ProcessingJobKind::Edf;
    if (stack.size() < 2) {
        result.status = ProcessingStartActionStatus::NotEnoughEdfFrames;
        result.message = L"Add at least two EDF frames before building EDF.";
        return result;
    }
    if (state.IsRunning()) {
        result.status = ProcessingStartActionStatus::AlreadyRunning;
        result.message = ProcessingStatusFormatter::FormatAlreadyRunning();
        return result;
    }

    if (before_begin) {
        before_begin();
    }
    state.ClearPending();
    if (remember_snapshot) {
        retry.RememberEdf(stack, options);
    }

    result.launch = state.Begin();
    result.status = ProcessingStartActionStatus::Started;
    result.can_start = true;
    result.message = ProcessingStatusFormatter::FormatStarted(result.kind);
    return result;
}
