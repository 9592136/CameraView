#pragma once

#include "Fluorescence.h"

#include <vector>

enum class FluorescenceBlendMode {
    Additive,
    Screen,
    Maximum
};

struct FluorescenceFusionOptions {
    FluorescenceBlendMode blend_mode = FluorescenceBlendMode::Additive;
};

class ChannelFusionEngine {
public:
    static ImageFrame Fuse(
        const std::vector<FluorescenceChannel>& channels,
        const FluorescenceFusionOptions& options = {});
};
