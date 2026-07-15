#pragma once

#include "ProcessingJobState.h"
#include "ProcessingRetryState.h"

#include <string>

enum class ProcessingRetryActionStatus {
    NoRetry,
    Ready
};

struct ProcessingRetryActionResult {
    ProcessingRetryActionStatus status = ProcessingRetryActionStatus::NoRetry;
    bool can_start = false;
    ProcessingJobKind kind = ProcessingJobKind::None;
    ProcessingRetryRequest request;
    std::wstring message;
};

class ProcessingRetryActions {
public:
    static ProcessingRetryActionResult Prepare(const ProcessingRetryState& retry);
};
