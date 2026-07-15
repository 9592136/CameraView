#pragma once

#include "../domain/ImageFrame.h"
#include "ImageStitcher.h"
#include "ProcessingJobState.h"
#include "ProcessingResultFrames.h"
#include "ProcessingRetryState.h"

#include <string>
#include <vector>

enum class ProcessingPanelActionStatus {
    Cleared,
    NoEdfCompositeFrame,
    EdfCompositeFrameShown,
    NoEdfFocusMap,
    EdfFocusMapShown
};

struct ProcessingPanelActionResult {
    ProcessingPanelActionStatus status = ProcessingPanelActionStatus::Cleared;
    bool changed = false;
    std::wstring message;
};

class ProcessingPanelActions {
public:
    static ProcessingPanelActionResult ShowEdfCompositeFrame(ProcessingResultFrames& frames);
    static ProcessingPanelActionResult ShowEdfFocusMap(ProcessingResultFrames& frames);

    static ProcessingPanelActionResult Clear(
        ProcessingJobState& state,
        std::vector<StitchTile>& stitch_tiles,
        std::vector<ImageFrame>& edf_stack,
        ProcessingRetryState& retry,
        ProcessingResultFrames& frames);
};
