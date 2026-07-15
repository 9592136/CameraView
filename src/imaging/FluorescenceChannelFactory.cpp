#include "FluorescenceChannelFactory.h"

#include "FluorescenceChannelSettings.h"
#include "FluorescenceFormatter.h"

#include <utility>

FluorescenceChannel FluorescenceChannelFactory::CreateFromFrame(
    const DyeProfile& dye,
    ImageFrame frame,
    std::size_t one_based_index)
{
    FluorescenceChannel channel;
    channel.name = FluorescenceFormatter::FormatDefaultChannelName(dye, one_based_index);
    channel.frame = std::move(frame);
    channel.color = dye.color;
    FluorescenceChannelSettings::Apply(channel, FluorescenceChannelSettings::Defaults(true));
    return channel;
}
