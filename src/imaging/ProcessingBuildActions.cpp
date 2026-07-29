#include "ProcessingBuildActions.h"

#include "ProcessingParameterRules.h"

#include <algorithm>

ProcessingBuildActionResult ProcessingBuildActions::PrepareStitch(
    const std::vector<StitchTile>& tiles,
    bool search_percent_valid,
    int search_percent,
    bool use_orb_registration)
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
    result.stitch_use_orb_registration = use_orb_registration;
    result.stitch_options.overlap_percent = ProcessingParameterRules::OverlapPercentFromSearch(search_percent);
    result.stitch_options.registration_method = use_orb_registration ?
        StitchRegistrationMethod::Micro :
        StitchRegistrationMethod::Phase;
    return result;
}

ProcessingBuildActionResult ProcessingBuildActions::PrepareStitch(
    const std::vector<StitchTile>& tiles,
    bool overlap_percent_valid,
    StitchProcessingOptions options)
{
    ProcessingBuildActionResult result;
    result.kind = ProcessingJobKind::Stitch;
    result.stitch_options = options;
    if (tiles.empty()) {
        result.status = ProcessingBuildActionStatus::NoStitchTiles;
        result.message = L"Add stitch tiles before building a stitched image.";
        return result;
    }
    if (!overlap_percent_valid) {
        result.status = ProcessingBuildActionStatus::InvalidStitchSearch;
        result.message = L"Estimated overlap must be 5-50 percent.";
        return result;
    }

    options.overlap_percent = ProcessingParameterRules::ClampStitchOverlapPercent(options.overlap_percent);
    options.grid_rows = std::clamp(options.grid_rows, 1, 50);
    options.grid_cols = std::clamp(options.grid_cols, 1, 50);
    if (options.layout_mode == StitchLayoutMode::Grid) {
        const std::size_t expected_tiles = static_cast<std::size_t>(options.grid_rows) *
            static_cast<std::size_t>(options.grid_cols);
        if (tiles.size() != expected_tiles) {
            result.status = ProcessingBuildActionStatus::NoStitchTiles;
            result.message = L"Grid stitch expects " + std::to_wstring(expected_tiles) +
                L" tile(s). Current: " + std::to_wstring(tiles.size()) + L".";
            return result;
        }
    }

    result.status = ProcessingBuildActionStatus::StitchReady;
    result.can_start = true;
    result.stitch_tiles = tiles;
    result.stitch_options = options;
    result.stitch_search_percent = ProcessingParameterRules::SearchPercentFromOverlap(options.overlap_percent);
    result.stitch_use_orb_registration = options.registration_method != StitchRegistrationMethod::Phase;
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
