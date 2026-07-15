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
    std::vector<ImageFrame> edf_stack;
    EdfOptions edf_options;

    bool CanRetry() const;
};

class ProcessingRetryState {
public:
    ProcessingRetryState();

    void Clear();
    void RememberStitch(std::vector<StitchTile> tiles, int search_percent);
    void RememberEdf(std::vector<ImageFrame> stack, EdfOptions options);
    ProcessingRetryRequest Request() const;

private:
    ProcessingRetryRequest request_;
};
