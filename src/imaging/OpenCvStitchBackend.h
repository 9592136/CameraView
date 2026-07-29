#pragma once

#include "ImageStitcher.h"

#include <atomic>
#include <functional>
#include <vector>

struct OpenCvStitchResult {
    bool available = false;
    bool succeeded = false;
    ImageFrame image;
    std::vector<StitchTile> positioned_tiles;
    int constraint_count = 0;
};

class OpenCvStitchBackend {
public:
    static bool IsAvailable();

    static OpenCvStitchResult Stitch(
        const std::vector<StitchTile>& tiles,
        StitchProcessingOptions options,
        const std::atomic_bool* cancel_requested = nullptr,
        const std::function<void(int)>& progress_callback = {});
};