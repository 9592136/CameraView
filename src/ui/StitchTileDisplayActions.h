#pragma once

#include "../imaging/ImageStitcher.h"

#include <string>
#include <vector>

class StitchTileDisplayActions {
public:
    static std::wstring TileLine(const StitchTile& tile, std::size_t index);
    static std::vector<std::wstring> TileLines(const std::vector<StitchTile>& tiles);
};
