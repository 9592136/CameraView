#pragma once

#include "../domain/ImageFrame.h"
#include "../imaging/ImageStitcher.h"
#include "../imaging/ProcessingResultFrames.h"

#include <cstddef>
#include <string>
#include <vector>

enum class ProcessingQueueActionStatus {
    InvalidStitchSearch,
    StitchNotAdded,
    StitchAdded,
    EdfNotAdded,
    EdfAdded
};

struct ProcessingQueueActionResult {
    ProcessingQueueActionStatus status = ProcessingQueueActionStatus::StitchNotAdded;
    bool changed = false;
    bool preview_changed = false;
    int stitch_search_percent = 0;
    std::size_t stitch_tile_count = 0;
    std::size_t edf_frame_count = 0;
    std::wstring message;
};

class ProcessingQueueActions {
public:
    static ProcessingQueueActionResult AddStitchTile(
        std::vector<StitchTile>& tiles,
        ProcessingResultFrames& frames,
        ImageFrame frame,
        const std::wstring& search_percent_text,
        int current_search_percent);

    static ProcessingQueueActionResult AddEdfFrame(
        std::vector<ImageFrame>& stack,
        ProcessingResultFrames& frames,
        ImageFrame frame);
};
