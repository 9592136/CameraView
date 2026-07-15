#include "StitchTilePlacementPlanner.h"

#include "ProcessingParameterRules.h"

#include <utility>

StitchTilePlacementResult StitchTilePlacementPlanner::PlaceNext(
    ImageFrame frame,
    const std::vector<StitchTile>& existing_tiles,
    int search_percent)
{
    StitchTilePlacementResult result;
    result.tile.frame = std::move(frame);

    if (existing_tiles.empty()) {
        return result;
    }

    const StitchTile& previous = existing_tiles.back();
    const StitchSearchRadius search_radius =
        ProcessingParameterRules::RegistrationSearchRadius(
            previous.frame.width,
            previous.frame.height,
            search_percent);
    result.registration = ImageRegistration::EstimateTranslation(
        previous.frame,
        result.tile.frame,
        search_radius.x,
        search_radius.y);
    if (result.registration.valid) {
        result.tile.offset_x = previous.offset_x + result.registration.dx;
        result.tile.offset_y = previous.offset_y + result.registration.dy;
        result.registered = true;
        return result;
    }

    result.tile.offset_x = previous.offset_x + previous.frame.width;
    result.tile.offset_y = previous.offset_y;
    return result;
}
