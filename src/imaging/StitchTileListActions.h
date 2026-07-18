#pragma once

#include "ImageRegistration.h"
#include "ImageStitcher.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

enum class StitchTileListActionStatus {
    NoFrame,
    Added,
    NoSelection,
    Deleted,
    Empty,
    Cleared
};

struct StitchTileListActionResult {
    StitchTileListActionStatus status = StitchTileListActionStatus::NoFrame;
    bool changed = false;
    bool registered = false;
    bool estimated = false;
    std::size_t tile_count = 0;
    TranslationOffset registration;
    std::optional<int> next_selection;
    std::wstring message;
};

class StitchTileListActions {
public:
    static StitchTileListActionResult AddCurrentFrame(
        std::vector<StitchTile>& tiles,
        ImageFrame frame,
        int search_percent);

    static StitchTileListActionResult AddFrames(
        std::vector<StitchTile>& tiles,
        std::vector<ImageFrame> frames,
        int search_percent);

    static StitchTileListActionResult DeleteSelected(
        std::vector<StitchTile>& tiles,
        int selection);

    static StitchTileListActionResult Clear(std::vector<StitchTile>& tiles);
};
