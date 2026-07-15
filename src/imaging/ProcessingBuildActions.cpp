#include "ProcessingBuildActions.h"

ProcessingBuildActionResult ProcessingBuildActions::PrepareStitch(
    const std::vector<StitchTile>& tiles,
    bool search_percent_valid,
    int search_percent)
{
    ProcessingBuildActionResult result;
    result.kind = ProcessingJobKind::Stitch;
    if (tiles.empty()) {
        result.status = ProcessingBuildActionStatus::NoStitchTiles;
        result.message = L"Add stitch tiles before building a stitched image.";
        return result;
    }
    if (!search_percent_valid) {
        result.status = ProcessingBuildActionStatus::InvalidStitchSearch;
        result.message = L"Stitch search must be 5-100 percent.";
        return result;
    }

    result.status = ProcessingBuildActionStatus::StitchReady;
    result.can_start = true;
    result.stitch_tiles = tiles;
    result.stitch_search_percent = search_percent;
    return result;
}

ProcessingBuildActionResult ProcessingBuildActions::PrepareEdf(
    const std::vector<ImageFrame>& stack,
    bool focus_radius_valid,
    int focus_radius)
{
    ProcessingBuildActionResult result;
    result.kind = ProcessingJobKind::Edf;
    if (stack.size() < 2) {
        result.status = ProcessingBuildActionStatus::NotEnoughEdfFrames;
        result.message = L"Add at least two EDF frames before building EDF.";
        return result;
    }
    if (!focus_radius_valid) {
        result.status = ProcessingBuildActionStatus::InvalidEdfRadius;
        result.message = L"EDF radius must be 1-16.";
        return result;
    }

    result.status = ProcessingBuildActionStatus::EdfReady;
    result.can_start = true;
    result.edf_stack = stack;
    result.edf_options.focus_radius = focus_radius;
    return result;
}
