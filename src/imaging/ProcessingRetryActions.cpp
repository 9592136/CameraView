#include "ProcessingRetryActions.h"

#include "ProcessingStatusFormatter.h"

ProcessingRetryActionResult ProcessingRetryActions::Prepare(const ProcessingRetryState& retry)
{
    ProcessingRetryActionResult result;
    result.request = retry.Request();
    result.kind = result.request.kind;

    if (!result.request.CanRetry()) {
        result.message = ProcessingStatusFormatter::FormatNoRetry(result.kind);
        return result;
    }

    result.status = ProcessingRetryActionStatus::Ready;
    result.can_start = true;
    result.message = ProcessingStatusFormatter::FormatRetryStarted(result.kind);
    return result;
}
