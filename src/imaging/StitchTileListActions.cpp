#include "StitchTileListActions.h"

#include "StitchTilePlacementPlanner.h"

#include <string>
#include <utility>

StitchTileListActionResult StitchTileListActions::AddCurrentFrame(
    std::vector<StitchTile>& tiles,
    ImageFrame frame,
    int search_percent)
{
    if (!frame.IsValid()) {
        StitchTileListActionResult result;
        result.message = L"No image frame to add as a stitch tile.";
        return result;
    }

    StitchTilePlacementResult placement =
        StitchTilePlacementPlanner::PlaceNext(std::move(frame), tiles, search_percent);
    tiles.push_back(std::move(placement.tile));

    StitchTileListActionResult result;
    result.status = StitchTileListActionStatus::Added;
    result.changed = true;
    result.registered = placement.registered;
    result.tile_count = tiles.size();
    result.registration = placement.registration;
    if (result.registered) {
        result.message =
            L"Stitch tile added with offset " +
            std::to_wstring(result.registration.dx) + L"," +
            std::to_wstring(result.registration.dy) + L".";
    } else {
        result.message = L"Stitch tile added: " + std::to_wstring(result.tile_count) + L".";
    }
    return result;
}
