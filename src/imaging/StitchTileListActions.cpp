#include "StitchTileListActions.h"

#include "StitchTilePlacementPlanner.h"

#include <algorithm>
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
    result.estimated = placement.estimated;
    result.tile_count = tiles.size();
    result.registration = placement.registration;
    if (result.registered) {
        result.message =
            L"Stitch tile added with offset " +
            std::to_wstring(result.registration.dx) + L"," +
            std::to_wstring(result.registration.dy) + L".";
    } else if (result.estimated) {
        result.message =
            L"Stitch tile added with fast estimated position. Stitch will refine alignment.";
    } else {
        result.message = L"Stitch tile added: " + std::to_wstring(result.tile_count) + L".";
    }
    return result;
}

StitchTileListActionResult StitchTileListActions::AddFrames(
    std::vector<StitchTile>& tiles,
    std::vector<ImageFrame> frames,
    int search_percent)
{
    StitchTileListActionResult result;
    std::size_t added_count = 0;
    for (ImageFrame& frame : frames) {
        StitchTileListActionResult add_result =
            AddCurrentFrame(tiles, std::move(frame), search_percent);
        if (!add_result.changed) {
            continue;
        }
        ++added_count;
        result.registered = result.registered || add_result.registered;
        result.estimated = result.estimated || add_result.estimated;
        result.registration = add_result.registration;
    }

    result.tile_count = tiles.size();
    if (added_count == 0) {
        result.status = StitchTileListActionStatus::NoFrame;
        result.message = L"No image frame to add as a stitch tile.";
        return result;
    }

    result.status = StitchTileListActionStatus::Added;
    result.changed = true;
    result.message =
        L"Stitch tiles added: " + std::to_wstring(added_count) +
        L". Total: " + std::to_wstring(result.tile_count) + L".";
    return result;
}

StitchTileListActionResult StitchTileListActions::DeleteSelected(
    std::vector<StitchTile>& tiles,
    int selection)
{
    StitchTileListActionResult result;
    result.tile_count = tiles.size();
    if (selection < 0 || selection >= static_cast<int>(tiles.size())) {
        result.status = StitchTileListActionStatus::NoSelection;
        result.message = tiles.empty()
            ? L"No stitch tiles to delete."
            : L"Select a stitch tile to delete.";
        return result;
    }

    tiles.erase(tiles.begin() + selection);
    result.status = StitchTileListActionStatus::Deleted;
    result.changed = true;
    result.tile_count = tiles.size();
    if (!tiles.empty()) {
        result.next_selection = std::min(selection, static_cast<int>(tiles.size()) - 1);
    }
    result.message = L"Stitch tile deleted. " + std::to_wstring(result.tile_count) + L" tile(s) remain.";
    return result;
}

StitchTileListActionResult StitchTileListActions::Clear(std::vector<StitchTile>& tiles)
{
    StitchTileListActionResult result;
    result.tile_count = tiles.size();
    if (tiles.empty()) {
        result.status = StitchTileListActionStatus::Empty;
        result.message = L"No stitch tiles to clear.";
        return result;
    }

    tiles.clear();
    result.status = StitchTileListActionStatus::Cleared;
    result.changed = true;
    result.tile_count = 0;
    result.message = L"Stitch tiles cleared.";
    return result;
}
