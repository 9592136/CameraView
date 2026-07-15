#pragma once

#include "Fluorescence.h"

#include <cstddef>
#include <optional>

struct FluorescenceChannelDisplaySettings {
    bool visible = true;
    unsigned char black_level = 0;
    unsigned char white_level = 255;
};

class FluorescenceChannelSettings {
public:
    static FluorescenceChannelDisplaySettings Defaults(bool visible = true);
    static std::optional<std::size_t> IndexAtSelection(int selection, std::size_t channel_count);
    static std::optional<FluorescenceChannelDisplaySettings> FromLevels(
        bool visible,
        int black_level,
        int white_level);
    static FluorescenceChannelDisplaySettings FromChannel(const FluorescenceChannel& channel);
    static void Apply(FluorescenceChannel& channel, const FluorescenceChannelDisplaySettings& settings);
};
