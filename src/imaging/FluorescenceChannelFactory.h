#pragma once

#include "Fluorescence.h"

#include <cstddef>

class FluorescenceChannelFactory {
public:
    static FluorescenceChannel CreateFromFrame(
        const DyeProfile& dye,
        ImageFrame frame,
        std::size_t one_based_index);
};
