#include "ProcessingQueueActions.h"

#include "../imaging/EdfStackListActions.h"
#include "../imaging/StitchTileListActions.h"
#include "ProcessingBuildInputActions.h"

#include <utility>

ProcessingQueueActionResult ProcessingQueueActions::AddStitchTile(
    std::vector<StitchTile>& tiles,
    ProcessingResultFrames& frames,
    ImageFrame frame,
    const std::wstring& search_percent_text,
    int current_search_percent)
{
    ProcessingQueueActionResult result;
    result.stitch_search_percent = current_search_percent;

    const ProcessingIntegerInputResult search_input =
        ProcessingBuildInputActions::StitchSearchForNextTile(
            frame.IsValid() && !tiles.empty(),
            search_percent_text,
            current_search_percent);
    if (!search_input.accepted) {
        result.status = ProcessingQueueActionStatus::InvalidStitchSearch;
        result.message = search_input.message;
        return result;
    }

    result.stitch_search_percent = search_input.value;
    const StitchTileListActionResult tile_result =
        StitchTileListActions::AddCurrentFrame(tiles, std::move(frame), search_input.value);
    result.stitch_tile_count = tile_result.tile_count;
    result.message = tile_result.message;
    if (!tile_result.changed) {
        result.status = ProcessingQueueActionStatus::StitchNotAdded;
        return result;
    }

    result.preview_changed = frames.Clear();
    result.status = ProcessingQueueActionStatus::StitchAdded;
    result.changed = true;
    return result;
}

ProcessingQueueActionResult ProcessingQueueActions::AddEdfFrame(
    std::vector<ImageFrame>& stack,
    ProcessingResultFrames& frames,
    ImageFrame frame)
{
    ProcessingQueueActionResult result;
    const EdfStackListActionResult stack_result =
        EdfStackListActions::AddCurrentFrame(stack, std::move(frame));
    result.edf_frame_count = stack_result.frame_count;
    result.message = stack_result.message;
    if (!stack_result.changed) {
        result.status = ProcessingQueueActionStatus::EdfNotAdded;
        return result;
    }

    result.preview_changed = frames.Clear();
    result.status = ProcessingQueueActionStatus::EdfAdded;
    result.changed = true;
    return result;
}
