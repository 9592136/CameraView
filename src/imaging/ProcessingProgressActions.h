#pragma once

#include "ProcessingJobState.h"
#include "ProcessingProgressThrottle.h"

#include <atomic>
#include <memory>
#include <string>

enum class ProcessingProgressActionStatus {
    Canceled,
    Suppressed,
    Report
};

struct ProcessingProgressActionResult {
    ProcessingProgressActionStatus status = ProcessingProgressActionStatus::Suppressed;
    bool should_report = false;
    std::wstring message;
};

class ProcessingProgressActions {
public:
    ProcessingProgressActions(
        ProcessingJobKind kind,
        std::shared_ptr<std::atomic_bool> cancel_token,
        int step_percent = 10);

    ProcessingProgressActionResult Report(int percent);

private:
    ProcessingJobKind kind_ = ProcessingJobKind::None;
    std::shared_ptr<std::atomic_bool> cancel_token_;
    ProcessingProgressThrottle throttle_;
};
