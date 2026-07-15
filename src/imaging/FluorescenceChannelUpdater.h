#pragma once

#include "Fluorescence.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

enum class FluorescenceChannelUpdateStatus {
    Applied,
    NoSelection,
    RangeOutOfBounds,
    InvalidRangeOrder
};

struct FluorescenceChannelUpdateResult {
    FluorescenceChannelUpdateStatus status = FluorescenceChannelUpdateStatus::NoSelection;
    bool applied = false;
    std::optional<std::size_t> index;
    std::wstring message;
};

class FluorescenceChannelUpdater {
public:
    static FluorescenceChannelUpdateResult Apply(
        std::vector<FluorescenceChannel>& channels,
        int selection,
        bool visible,
        int black_level,
        int white_level);
};
