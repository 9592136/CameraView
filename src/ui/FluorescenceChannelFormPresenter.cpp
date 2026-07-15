#include "FluorescenceChannelFormPresenter.h"

#include "../imaging/FluorescenceChannelSettings.h"

namespace {

FluorescenceChannelFormValues FromSettings(const FluorescenceChannelDisplaySettings& settings)
{
    return FluorescenceChannelFormValues{
        settings.visible,
        std::to_wstring(settings.black_level),
        std::to_wstring(settings.white_level)
    };
}

} // namespace

FluorescenceChannelFormValues FluorescenceChannelFormPresenter::Empty()
{
    return FromSettings(FluorescenceChannelSettings::Defaults(false));
}

FluorescenceChannelFormValues FluorescenceChannelFormPresenter::FromChannel(
    const FluorescenceChannel& channel)
{
    return FromSettings(FluorescenceChannelSettings::FromChannel(channel));
}
