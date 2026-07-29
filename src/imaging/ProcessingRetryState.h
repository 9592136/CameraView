#pragma once

#include "../domain/ImageFrame.h"
#include "EdfProcessor.h"
#include "ImageStitcher.h"
#include "ProcessingJobState.h"

#include <vector>

struct ProcessingRetryRequest {
    ProcessingJobKind kind = ProcessingJobKind::None;
    std::vector<StitchTile> stitch_tiles;
    int stitch_search_percent = 0;
    bool stitch_use_orb_registration = true;
    StitchProcessingOptions stitch_options;
    std::vector<ImageFrame> edf_stack;
    EdfOptions edf_options;

    bool CanRetry() const;
};

class ProcessingRetryState {
public:
    ProcessingRetryState();

    void Clear();
    void RememberStitch(
        std::vector<StitchTile> tiles,
        int search_percent,
        bool use_orb_registration = true);
    void RememberStitch(
        std::vector<StitchTile> tiles,
        StitchProcessingOptions options);
    void RememberEdf(std::vector<ImageFrame> stack, EdfOptions options);
    ProcessingRetryRequest Request() const;

private:
    ProcessingRetryRequest request_;
};
