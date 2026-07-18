#pragma once

#include "ImageRegistration.h"
#include "ImageStitcher.h"

#include <vector>

struct StitchTilePlacementResult {
    StitchTile tile;
    TranslationOffset registration;
    bool registered = false;
    bool estimated = false;
};

class StitchTilePlacementPlanner {
public:
    static StitchTilePlacementResult PlaceNext(
        ImageFrame frame,
        const std::vector<StitchTile>& existing_tiles,
        int search_percent);
};
