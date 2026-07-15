#include "DyeProfileFormPresenter.h"

#include <iomanip>
#include <sstream>

DyeProfileFormValues DyeProfileFormPresenter::Empty()
{
    return DyeProfileFormValues{
        L"",
        L"0",
        L"0",
        L"255",
        L"255",
        L"255"
    };
}

DyeProfileFormValues DyeProfileFormPresenter::FromDye(const DyeProfile& dye)
{
    return DyeProfileFormValues{
        dye.name,
        FormatDouble(dye.excitation_nm, 0),
        FormatDouble(dye.emission_nm, 0),
        std::to_wstring(dye.color.r),
        std::to_wstring(dye.color.g),
        std::to_wstring(dye.color.b)
    };
}

std::wstring DyeProfileFormPresenter::FormatDouble(double value, int precision)
{
    std::wostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}
