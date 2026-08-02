#pragma once

#include "../domain/ImageFrame.h"

#include <atomic>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

struct SmartTargetRegion {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct SmartTargetMatch {
    SmartTargetRegion region;
    double confidence = 0.0;
    std::size_t sample_index = 0;
};

struct SmartTargetCountOptions {
    double similarity_threshold = 0.82;
    double scale_tolerance = 0.15;
    int scale_steps = 5;
    double duplicate_iou_threshold = 0.25;
    int maximum_results = 5000;
};

struct SmartTargetCountResult {
    bool available = false;
    bool succeeded = false;
    bool canceled = false;
    std::vector<SmartTargetMatch> matches;
    int evaluated_template_count = 0;
    double elapsed_milliseconds = 0.0;
    std::wstring message;
};

class SmartTargetCounter {
public:
    static bool IsAvailable();

    static SmartTargetCountResult Count(
        const ImageFrame& frame,
        const std::vector<SmartTargetRegion>& samples,
        SmartTargetCountOptions options = {},
        const std::atomic_bool* cancel_requested = nullptr,
        const std::function<void(int)>& progress_callback = {});
};
