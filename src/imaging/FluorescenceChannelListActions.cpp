#include "FluorescenceChannelListActions.h"

#include "FluorescenceChannelFactory.h"

#include <utility>

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
