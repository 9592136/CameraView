#pragma once

#include "EdfProcessor.h"
#include "ImageStitcher.h"
#include "ProcessingJobState.h"

#include <atomic>
#include <functional>
#include <vector>

class ProcessingJobExecutor {
public:
    using ProgressCallback = std::function<void(int)>;

    static ProcessingJobResult RunStitch(
        unsigned long long job_id,
        std::vector<StitchTile> tiles,
        int search_percent,
        const std::atomic_bool* cancel_requested = nullptr,
        const ProgressCallback& progress_callback = {});

    static ProcessingJobResult RunEdf(
        unsigned long long job_id,
        std::vector<ImageFrame> stack,
        EdfOptions options,
        const std::atomic_bool* cancel_requested = nullptr,
        const ProgressCallback& progress_callback = {});
};
