#include "ProcessingProgressActions.h"

#include "ProcessingStatusFormatter.h"

ProcessingProgressActions::ProcessingProgressActions(
    ProcessingJobKind kind,
    std::shared_ptr<std::atomic_bool> cancel_token,
    int step_percent)
    : kind_(kind),
      cancel_token_(std::move(cancel_token)),
      throttle_(step_percent)
{
}

ProcessingProgressActionResult ProcessingProgressActions::Report(int percent)
{
    ProcessingProgressActionResult result;
    if (cancel_token_ && cancel_token_->load()) {
        result.status = ProcessingProgressActionStatus::Canceled;
        return result;
    }
    if (!throttle_.ShouldReport(percent)) {
        result.status = ProcessingProgressActionStatus::Suppressed;
        return result;
    }

    result.status = ProcessingProgressActionStatus::Report;
    result.should_report = true;
    result.message = ProcessingStatusFormatter::FormatProgress(kind_, percent);
    return result;
}
