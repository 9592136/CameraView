#pragma once

#include "ImageStitcher.h"

#include <cstddef>
#include <string>
#include <vector>

struct LiveStitchCaptureOptions {
    int max_registration_edge = 256;
    int min_movement_percent = 20;
    int min_overlap_percent = 15;
    int search_percent = 85;
    bool fast_mode = false;
    int reference_tile_count = 1;
};

struct LiveStitchCaptureDecision {
    bool should_capture = false;
    bool first_tile = false;
    bool registration_valid = false;
    bool match_missing = false;
    bool out_of_range_warning = false;
    int dx = 0;
    int dy = 0;
    int tile_offset_x = 0;
    int tile_offset_y = 0;
    std::size_t reference_tile_index = 0;
    int movement_percent = 0;
    int overlap_percent = 0;
    double confidence = 0.0;
    std::wstring message;
};

class LiveStitchCapturePlanner {
public:
    static LiveStitchCaptureDecision Evaluate(
        const std::vector<StitchTile>& tiles,
        const ImageFrame& candidate,
        LiveStitchCaptureOptions options = {});
};
