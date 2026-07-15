#include "FluorescenceFormatter.h"

#include <iomanip>
#include <sstream>

std::wstring FluorescenceFormatter::FormatDefaultChannelName(
    const DyeProfile& dye,
    std::size_t one_based_index)
{
    return dye.name + L" " + std::to_wstring(one_based_index);
}

std::wstring FluorescenceFormatter::FormatDyeLabel(const DyeProfile& dye)
{
    return dye.name + L"  " +
           FormatDouble(dye.excitation_nm, 0) + L"/" +
           FormatDouble(dye.emission_nm, 0) + L" nm";
}

std::wstring FluorescenceFormatter::FormatChannelLine(const FluorescenceChannel& channel)
{
    return (channel.visible ? L"[on] " : L"[off] ") +
           channel.name + L"  " +
           FormatFrameSize(channel.frame) +
           L"  " + std::to_wstring(channel.black_level) +
           L"-" + std::to_wstring(channel.white_level);
}

std::wstring FluorescenceFormatter::FormatDouble(double value, int precision)
{
    std::wostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

std::wstring FluorescenceFormatter::FormatFrameSize(const ImageFrame& frame)
{
    if (!frame.IsValid()) {
        return L"no frame";
    }
    return std::to_wstring(frame.width) + L"x" + std::to_wstring(frame.height);
}
