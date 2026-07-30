#pragma once

#include "ImageStitcher.h"
#include "ProcessingJobState.h"

#include <memory>
#include <vector>

struct LiveStitchPreviewTile {
    std::shared_ptr<const ImageFrame> frame;
    int offset_x = 0;
    int offset_y = 0;
    bool estimated_position = false;
};

struct LiveStitchPreviewOptions {
    int max_preview_edge = 1600;
    int overlap_percent = 25;
    StitchBlendMode blend_mode = StitchBlendMode::Linear;
};

struct LiveStitchPreviewResult {
    ImageFrame image;
    StitchResultMetadata metadata;
    int scale = 1;
};

class LiveStitchPreviewBuilder {
public:
    static LiveStitchPreviewResult Build(
        const std::vector<LiveStitchPreviewTile>& tiles,
        LiveStitchPreviewOptions options = {});

    static LiveStitchPreviewResult Build(
        const std::vector<StitchTile>& tiles,
        LiveStitchPreviewOptions options = {});
};
