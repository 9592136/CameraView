#pragma once

#include "EdfProcessor.h"
#include "ImageStitcher.h"
#include "ProcessingJobState.h"
#include "ProcessingRetryState.h"

#include <functional>
#include <string>
#include <vector>

enum class ProcessingStartActionStatus {
    NoStitchTiles,
    NotEnoughEdfFrames,
    AlreadyRunning,
    Started
};

struct ProcessingStartActionResult {
    ProcessingStartActionStatus status = ProcessingStartActionStatus::NoStitchTiles;
    bool can_start = false;
    ProcessingJobKind kind = ProcessingJobKind::None;
    ProcessingJobLaunch launch;
    std::wstring message;
};

class ProcessingStartActions {
public:
    using BeforeBeginCallback = std::function<void()>;

    static ProcessingStartActionResult StartStitch(
        ProcessingJobState& state,
        ProcessingRetryState& retry,
        const std::vector<StitchTile>& tiles,
        int search_percent,
        bool remember_snapshot,
        const BeforeBeginCallback& before_begin);

    static ProcessingStartActionResult StartEdf(
        ProcessingJobState& state,
        ProcessingRetryState& retry,
        const std::vector<ImageFrame>& stack,
        EdfOptions options,
        bool remember_snapshot,
        const BeforeBeginCallback& before_begin);
};
