#include "EdfStackListActions.h"

#include <string>
#include <utility>

EdfStackListActionResult EdfStackListActions::AddCurrentFrame(
    std::vector<ImageFrame>& stack,
    ImageFrame frame)
{
    if (!frame.IsValid()) {
        EdfStackListActionResult result;
        result.message = L"No image frame to add to the EDF stack.";
        return result;
    }

    stack.push_back(std::move(frame));

    EdfStackListActionResult result;
    result.status = EdfStackListActionStatus::Added;
    result.changed = true;
    result.frame_count = stack.size();
    result.message = L"EDF frame added: " + std::to_wstring(result.frame_count) + L".";
    return result;
}
