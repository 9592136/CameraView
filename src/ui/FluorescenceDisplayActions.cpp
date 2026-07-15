#include "FluorescenceDisplayActions.h"

#include "../imaging/DyeLibrary.h"
#include "../imaging/FluorescenceChannelSettings.h"
#include "../imaging/FluorescenceFormatter.h"

std::vector<std::wstring> FluorescenceDisplayActions::DyeLabels(const std::vector<DyeProfile>& dyes)
{
    std::vector<std::wstring> labels;
    labels.reserve(dyes.size());
    for (const DyeProfile& dye : dyes) {
        labels.push_back(FluorescenceFormatter::FormatDyeLabel(dye));
    }
    return labels;
}

DyeProfile FluorescenceDisplayActions::SelectedDye(
    const std::vector<DyeProfile>& dyes,
    int selected_index)
{
    if (dyes.empty()) {
        return DyeLibrary::FallbackDye();
    }

    const std::optional<std::size_t> index = SelectedDyeIndex(dyes, selected_index);
    return dyes[index.value_or(0)];
}

std::optional<std::size_t> FluorescenceDisplayActions::SelectedDyeIndex(
    const std::vector<DyeProfile>& dyes,
    int selected_index)
{
    return DyeLibrary::IndexAtSelection(selected_index, dyes.size());
}

std::vector<std::wstring> FluorescenceDisplayActions::ChannelLines(
    const std::vector<FluorescenceChannel>& channels)
{
    std::vector<std::wstring> lines;
    lines.reserve(channels.size());
    for (const FluorescenceChannel& channel : channels) {
        lines.push_back(FluorescenceFormatter::FormatChannelLine(channel));
    }
    return lines;
}

std::optional<std::size_t> FluorescenceDisplayActions::SelectedChannelIndex(
    const std::vector<FluorescenceChannel>& channels,
    int selected_index)
{
    return FluorescenceChannelSettings::IndexAtSelection(selected_index, channels.size());
}
