#include "FluorescenceChannelSettings.h"

FluorescenceChannelDisplaySettings FluorescenceChannelSettings::Defaults(bool visible)
{
    FluorescenceChannelDisplaySettings settings;
    settings.visible = visible;
    return settings;
}

std::optional<std::size_t> FluorescenceChannelSettings::IndexAtSelection(
    int selection,
    std::size_t channel_count)
{
    if (selection < 0 || static_cast<std::size_t>(selection) >= channel_count) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(selection);
}

std::optional<FluorescenceChannelDisplaySettings> FluorescenceChannelSettings::FromLevels(
    bool visible,
    int black_level,
    int white_level)
{
    if (black_level < 0 || black_level > 255 ||
        white_level < 0 || white_level > 255 ||
        white_level <= black_level) {
        return std::nullopt;
    }

    FluorescenceChannelDisplaySettings settings;
    settings.visible = visible;
    settings.black_level = static_cast<unsigned char>(black_level);
    settings.white_level = static_cast<unsigned char>(white_level);
    return settings;
}

FluorescenceChannelDisplaySettings FluorescenceChannelSettings::FromChannel(
    const FluorescenceChannel& channel)
{
    FluorescenceChannelDisplaySettings settings;
    settings.visible = channel.visible;
    settings.black_level = channel.black_level;
    settings.white_level = channel.white_level;
    return settings;
}

void FluorescenceChannelSettings::Apply(
    FluorescenceChannel& channel,
    const FluorescenceChannelDisplaySettings& settings)
{
    channel.visible = settings.visible;
    channel.black_level = settings.black_level;
    channel.white_level = settings.white_level;
}
