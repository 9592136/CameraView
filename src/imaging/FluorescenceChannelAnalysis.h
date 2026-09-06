#pragma once

#include "Fluorescence.h"

#include <array>
#include <cstdint>
#include <string>

enum class FluorescenceExposureState {
    NoData,
    Underexposed,
    LowContrast,
    Balanced,
    Saturated
};

struct FluorescenceChannelStatistics {
    std::array<std::uint64_t, 256> histogram{};
    std::uint64_t pixel_count = 0;
    unsigned char minimum = 0;
    unsigned char maximum = 0;
    unsigned char suggested_black_level = 0;
    unsigned char suggested_white_level = 255;
    double mean = 0.0;
    double clipped_fraction = 0.0;
    FluorescenceExposureState exposure = FluorescenceExposureState::NoData;

    bool IsValid() const { return pixel_count > 0; }
};

class FluorescenceChannelAnalysis {
public:
    // Robust defaults for sparse fluorescence images: discard the darkest 1%
    // and the brightest 0.2% when proposing display levels. Raw pixels remain
    // unchanged; the result only controls visualization.
    static FluorescenceChannelStatistics Analyze(
        const ImageFrame& frame,
        double low_percentile = 0.01,
        double high_percentile = 0.998);

    static bool ApplySuggestedLevels(
        FluorescenceChannel& channel,
        const FluorescenceChannelStatistics& statistics);

    static std::wstring ExposureLabel(FluorescenceExposureState state);
};
