#pragma once

#include "../domain/ImageFrame.h"

#include <atomic>
#include <functional>
#include <vector>

struct StitchTile {
    ImageFrame frame;
    int offset_x = 0;
    int offset_y = 0;
};

struct StitchOptimizationOptions {
    int search_radius_x = 16;
    int search_radius_y = 16;
    int iterations = 25;
};

struct StitchOptimizationResult {
    std::vector<StitchTile> tiles;
    int constraint_count = 0;
    bool optimized = false;
};

class ImageStitcher {
public:
    static StitchOptimizationResult OptimizeTileOffsets(
        const std::vector<StitchTile>& tiles,
        StitchOptimizationOptions options = {},
        const std::atomic_bool* cancel_requested = nullptr,
        const std::function<void(int)>& progress_callback = {});

    static ImageFrame StitchAverage(
        const std::vector<StitchTile>& tiles,
        const std::atomic_bool* cancel_requested = nullptr,
        const std::function<void(int)>& progress_callback = {});
};
