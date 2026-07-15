#pragma once

#include "EdfProcessor.h"
#include "ImageStitcher.h"
#include "ProcessingJobState.h"

#include <functional>
#include <string>
#include <thread>
#include <vector>

class ProcessingWorkerActions {
public:
    using StatusCallback = std::function<void(const std::wstring&)>;
    using PublishCallback = std::function<void(ProcessingJobResult)>;

    static std::thread StartStitch(
        ProcessingJobLaunch launch,
        std::vector<StitchTile> tiles,
        int search_percent,
        StatusCallback status_callback,
        PublishCallback publish_callback);

    static std::thread StartEdf(
        ProcessingJobLaunch launch,
        std::vector<ImageFrame> stack,
        EdfOptions options,
        StatusCallback status_callback,
        PublishCallback publish_callback);
};
