#pragma once

#include "ProcessingJobState.h"
#include "ProcessingResultFrames.h"

#include <string>

enum class ProcessingResultActionStatus {
    NoResult,
    StaleResult,
    Failed,
    Applied
};

struct ProcessingResultActionResult {
    ProcessingResultActionStatus status = ProcessingResultActionStatus::NoResult;
    bool handled = false;
    bool changed = false;
    std::wstring message;
};

class ProcessingResultActions {
public:
    static ProcessingResultActionResult ApplyPending(
        ProcessingJobState& state,
        ProcessingResultFrames& frames);
};
