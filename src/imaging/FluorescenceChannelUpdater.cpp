#include "FluorescenceChannelUpdater.h"

#include "FluorescenceChannelSettings.h"

FluorescenceChannelUpdateResult FluorescenceChannelUpdater::Apply(
    std::vector<FluorescenceChannel>& channels,
    int selection,
    bool visible,
    int black_level,
    int white_level)
{
    const std::optional<std::size_t> index =
        FluorescenceChannelSettings::IndexAtSelection(selection, channels.size());
    if (!index) {
        return {
            FluorescenceChannelUpdateStatus::NoSelection,
            false,
            std::nullopt,
            L"Select a fluorescence channel first."
        };
    }

    if (black_level < 0 || black_level > 255 || white_level < 0 || white_level > 255) {
        return {
            FluorescenceChannelUpdateStatus::RangeOutOfBounds,
            false,
            index,
            L"Channel range must be 0-255."
        };
    }

    const std::optional<FluorescenceChannelDisplaySettings> settings =
        FluorescenceChannelSettings::FromLevels(visible, black_level, white_level);
    if (!settings) {
        return {
            FluorescenceChannelUpdateStatus::InvalidRangeOrder,
            false,
            index,
            L"White level must be greater than black level."
        };
    }

    FluorescenceChannelSettings::Apply(channels[*index], *settings);
    return {
        FluorescenceChannelUpdateStatus::Applied,
        true,
        index,
        L"Fluorescence channel updated."
    };
}
