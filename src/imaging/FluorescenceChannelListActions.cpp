#include "FluorescenceChannelListActions.h"

#include "FluorescenceChannelFactory.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace {

bool IsValidSelection(int selection, std::size_t channel_count)
{
    return selection >= 0 && static_cast<std::size_t>(selection) < channel_count;
}

} // namespace

FluorescenceChannelListActionResult FluorescenceChannelListActions::AddCurrentFrame(
    std::vector<FluorescenceChannel>& channels,
    ImageFrame frame,
    const DyeProfile& dye)
{
    FluorescenceChannelListActionResult result;
    if (!frame.IsValid()) {
        result.status = FluorescenceChannelListActionStatus::NoFrame;
        result.message = L"No image frame to add as a fluorescence channel.";
        return result;
    }

    const std::size_t new_index = channels.size();
    FluorescenceChannel channel = FluorescenceChannelFactory::CreateFromFrame(
        dye,
        std::move(frame),
        new_index + 1);
    channels.push_back(std::move(channel));

    result.status = FluorescenceChannelListActionStatus::Added;
    result.changed = true;
    result.show_fusion_preview = true;
    result.selected_index = new_index;
    result.message = L"Added fluorescence channel: " + dye.name + L".";
    return result;
}

FluorescenceChannelListActionResult FluorescenceChannelListActions::Clear(
    std::vector<FluorescenceChannel>& channels)
{
    channels.clear();

    FluorescenceChannelListActionResult result;
    result.status = FluorescenceChannelListActionStatus::Cleared;
    result.changed = true;
    result.show_fusion_preview = false;
    result.message = L"Fluorescence channels cleared.";
    return result;
}

FluorescenceChannelListActionResult FluorescenceChannelListActions::RemoveSelected(
    std::vector<FluorescenceChannel>& channels,
    int selection)
{
    FluorescenceChannelListActionResult result;
    if (!IsValidSelection(selection, channels.size())) {
        result.status = FluorescenceChannelListActionStatus::NoSelection;
        result.show_fusion_preview = !channels.empty();
        result.message = L"Select a fluorescence channel first.";
        return result;
    }

    const std::size_t index = static_cast<std::size_t>(selection);
    const std::wstring removed_name = channels[index].name;
    channels.erase(channels.begin() + static_cast<std::ptrdiff_t>(index));
    result.status = FluorescenceChannelListActionStatus::Removed;
    result.changed = true;
    result.show_fusion_preview = !channels.empty();
    if (!channels.empty()) {
        result.selected_index = std::min(index, channels.size() - 1);
    }
    result.message = L"Removed fluorescence channel: " + removed_name + L".";
    return result;
}

FluorescenceChannelListActionResult FluorescenceChannelListActions::ShowOnlySelected(
    std::vector<FluorescenceChannel>& channels,
    int selection)
{
    FluorescenceChannelListActionResult result;
    if (!IsValidSelection(selection, channels.size())) {
        result.status = FluorescenceChannelListActionStatus::NoSelection;
        result.show_fusion_preview = !channels.empty();
        result.message = L"Select a fluorescence channel first.";
        return result;
    }

    const std::size_t selected = static_cast<std::size_t>(selection);
    for (std::size_t index = 0; index < channels.size(); ++index) {
        channels[index].visible = index == selected;
    }
    result.status = FluorescenceChannelListActionStatus::Isolated;
    result.changed = true;
    result.show_fusion_preview = true;
    result.selected_index = selected;
    result.message = L"Showing only fluorescence channel: " + channels[selected].name + L".";
    return result;
}

FluorescenceChannelListActionResult FluorescenceChannelListActions::ShowAll(
    std::vector<FluorescenceChannel>& channels)
{
    FluorescenceChannelListActionResult result;
    for (FluorescenceChannel& channel : channels) {
        channel.visible = true;
    }
    result.status = FluorescenceChannelListActionStatus::ShownAll;
    result.changed = !channels.empty();
    result.show_fusion_preview = !channels.empty();
    result.message = channels.empty()
        ? L"No fluorescence channels to show."
        : L"All fluorescence channels are visible.";
    return result;
}
