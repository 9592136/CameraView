#pragma once

#include "ImageRegistration.h"
#include "ImageStitcher.h"

#include <cstddef>
#include <string>
#include <vector>

enum class StitchTileListActionStatus {
    NoFrame,
    Added
};

struct StitchTileListActionResult {
    StitchTileListActionStatus status = StitchTileListActionStatus::NoFrame;
    bool changed = false;
    bool registered = false;
    std::size_t tile_count = 0;
    TranslationOffset registration;
    std::wstring message;
};

class StitchTileListActions {
public:
    static StitchTileListActionResult AddCurrentFrame(
        std::vector<StitchTile>& tiles,
        ImageFrame frame,
        int search_percent);
};
