#include "ProcessingPanelActions.h"

#include "ProcessingStatusFormatter.h"

ProcessingPanelActionResult ProcessingPanelActions::ShowEdfCompositeFrame(ProcessingResultFrames& frames)
{
    ProcessingPanelActionResult result;
    if (!frames.ShowEdfCompositeFrame()) {
        result.status = ProcessingPanelActionStatus::NoEdfCompositeFrame;
        result.message = L"Build an EDF image before viewing the EDF image.";
        return result;
    }

    result.status = ProcessingPanelActionStatus::EdfCompositeFrameShown;
    result.changed = true;
    result.message = L"EDF image ready.";
    return result;
}

ProcessingPanelActionResult ProcessingPanelActions::ShowEdfFocusMap(ProcessingResultFrames& frames)
{
    ProcessingPanelActionResult result;
    if (!frames.ShowEdfFocusMap()) {
        result.status = ProcessingPanelActionStatus::NoEdfFocusMap;
        result.message = L"Build an EDF image before viewing the focus map.";
        return result;
    }

    result.status = ProcessingPanelActionStatus::EdfFocusMapShown;
    result.changed = true;
    result.message = L"EDF focus map ready.";
    return result;
}

ProcessingPanelActionResult ProcessingPanelActions::Clear(
    ProcessingJobState& state,
    std::vector<StitchTile>& stitch_tiles,
    std::vector<ImageFrame>& edf_stack,
    ProcessingRetryState& retry,
    ProcessingResultFrames& frames)
{
    state.RequestCancel();
    state.InvalidateActiveJob();
    stitch_tiles.clear();
    edf_stack.clear();
    retry.Clear();
    frames.Clear();

    ProcessingPanelActionResult result;
    result.status = ProcessingPanelActionStatus::Cleared;
    result.changed = true;
    result.message = ProcessingStatusFormatter::FormatCleared(state.IsRunning());
    return result;
}
