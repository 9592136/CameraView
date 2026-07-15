#include "ProcessingResultActions.h"

#include <utility>

ProcessingResultActionResult ProcessingResultActions::ApplyPending(
    ProcessingJobState& state,
    ProcessingResultFrames& frames)
{
    ProcessingJobResult result = state.TakePending();
    if (!result.has_result) {
        return {};
    }
    if (!state.IsCurrentResult(result)) {
        ProcessingResultActionResult action;
        action.status = ProcessingResultActionStatus::StaleResult;
        return action;
    }
    if (!result.succeeded) {
        ProcessingResultActionResult action;
        action.status = ProcessingResultActionStatus::Failed;
        action.handled = true;
        action.message = result.status;
        return action;
    }

    const std::wstring status = result.status;
    frames.Apply(std::move(result));

    ProcessingResultActionResult action;
    action.status = ProcessingResultActionStatus::Applied;
    action.handled = true;
    action.changed = true;
    action.message = status;
    return action;
}
