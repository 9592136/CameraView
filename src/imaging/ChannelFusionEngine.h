#pragma once

#include "Fluorescence.h"

#include <vector>

class ChannelFusionEngine {
public:
    static ImageFrame Fuse(const std::vector<FluorescenceChannel>& channels);
};
