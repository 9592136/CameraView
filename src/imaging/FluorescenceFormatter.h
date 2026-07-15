#pragma once

#include "Fluorescence.h"

#include <cstddef>
#include <string>

class FluorescenceFormatter {
public:
    static std::wstring FormatDefaultChannelName(const DyeProfile& dye, std::size_t one_based_index);
    static std::wstring FormatDyeLabel(const DyeProfile& dye);
    static std::wstring FormatChannelLine(const FluorescenceChannel& channel);

private:
    static std::wstring FormatDouble(double value, int precision);
    static std::wstring FormatFrameSize(const ImageFrame& frame);
};
