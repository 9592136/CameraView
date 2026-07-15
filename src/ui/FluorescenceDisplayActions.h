#pragma once

#include "../imaging/Fluorescence.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

class FluorescenceDisplayActions {
public:
    static std::vector<std::wstring> DyeLabels(const std::vector<DyeProfile>& dyes);
    static DyeProfile SelectedDye(const std::vector<DyeProfile>& dyes, int selected_index);
    static std::optional<std::size_t> SelectedDyeIndex(
        const std::vector<DyeProfile>& dyes,
        int selected_index);
    static std::vector<std::wstring> ChannelLines(const std::vector<FluorescenceChannel>& channels);
    static std::optional<std::size_t> SelectedChannelIndex(
        const std::vector<FluorescenceChannel>& channels,
        int selected_index);
};
