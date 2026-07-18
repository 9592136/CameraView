#include "StitchTileDisplayActions.h"

std::wstring StitchTileDisplayActions::TileLine(const StitchTile& tile, std::size_t index)
{
    return L"Tile " + std::to_wstring(index + 1) +
        L"  " + std::to_wstring(tile.frame.width) +
        L"x" + std::to_wstring(tile.frame.height) +
        L"  @ " + std::to_wstring(tile.offset_x) +
        L"," + std::to_wstring(tile.offset_y) +
        (tile.estimated_position ? L"  estimated" : L"");
}

std::vector<std::wstring> StitchTileDisplayActions::TileLines(const std::vector<StitchTile>& tiles)
{
    std::vector<std::wstring> lines;
    lines.reserve(tiles.size());
    for (std::size_t index = 0; index < tiles.size(); ++index) {
        lines.push_back(TileLine(tiles[index], index));
    }
    return lines;
}
