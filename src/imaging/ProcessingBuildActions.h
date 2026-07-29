#pragma once

#include "../domain/ImageFrame.h"
#include "EdfProcessor.h"
#include "ImageStitcher.h"
#include "ProcessingJobState.h"

#include <string>
#include <vector>

enum class ProcessingBuildActionStatus {
    NoStitchTiles,
    InvalidStitchSearch,
    StitchReady,
    NotEnoughEdfFrames,
    InvalidEdfRadius,
    EdfReady
};

struct ProcessingBuildActionResult {
    ProcessingBuildActionStatus status = ProcessingBuildActionStatus::NoStitchTiles;
    bool can_start = false;
    ProcessingJobKind kind = ProcessingJobKind::None;
    std::vector<StitchTile> stitch_tiles;
    int stitch_search_percent = 0;
    bool stitch_use_orb_registration = true;
    StitchProcessingOptions stitch_options;
    std::vector<ImageFrame> edf_stack;
    EdfOptions edf_options;
    std::wstring message;
};

class ProcessingBuildActions {
public:
    static ProcessingBuildActionResult PrepareStitch(
        const std::vector<StitchTile>& tiles,
        bool search_percent_valid,
        int search_percent,
        bool use_orb_registration = true);

    static ProcessingBuildActionResult PrepareStitch(
        const std::vector<StitchTile>& tiles,
        bool overlap_percent_valid,
        StitchProcessingOptions options);

    static ProcessingBuildActionResult PrepareEdf(
        const std::vector<ImageFrame>& stack,
        bool focus_radius_valid,
        int focus_radius);
};
