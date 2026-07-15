#pragma once

#include "../imaging/Fluorescence.h"

#include <string>

struct DyeProfileFormValues {
    std::wstring name;
    std::wstring excitation_nm;
    std::wstring emission_nm;
    std::wstring red;
    std::wstring green;
    std::wstring blue;
};

class DyeProfileFormPresenter {
public:
    static DyeProfileFormValues Empty();
    static DyeProfileFormValues FromDye(const DyeProfile& dye);

private:
    static std::wstring FormatDouble(double value, int precision);
};
