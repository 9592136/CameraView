#pragma once

#include "Fluorescence.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

enum class FluorescenceChannelListActionStatus {
    NoFrame,
    NoSelection,
    Added,
    Removed,
    Isolated,
    ShownAll,
    Cleared
};

struct FluorescenceChannelListActionResult {
    FluorescenceChannelListActionStatus status = FluorescenceChannelListActionStatus::NoFrame;
    bool changed = false;
    bool show_fusion_preview = false;
    std::optional<std::size_t> selected_index;
    std::wstring message;
};

class FluorescenceChannelListActions {
public:
    static FluorescenceChannelListActionResult AddCurrentFrame(
        std::vector<FluorescenceChannel>& channels,
        ImageFrame frame,
        const DyeProfile& dye);

    static FluorescenceChannelListActionResult Clear(
        std::vector<FluorescenceChannel>& channels);

    static FluorescenceChannelListActionResult RemoveSelected(
        std::vector<FluorescenceChannel>& channels,
        int selection);

    static FluorescenceChannelListActionResult ShowOnlySelected(
        std::vector<FluorescenceChannel>& channels,
        int selection);

    static FluorescenceChannelListActionResult ShowAll(
        std::vector<FluorescenceChannel>& channels);
};
