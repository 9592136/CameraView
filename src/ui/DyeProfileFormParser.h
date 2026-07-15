#pragma once

#include "../imaging/Fluorescence.h"

#include <optional>
#include <string>

enum class DyeProfileInputStatus {
    Valid,
    MissingName,
    InvalidWavelengths,
    InvalidColor
};

struct DyeProfileInput {
    std::wstring name;
    std::wstring excitation_nm;
    std::wstring emission_nm;
    std::wstring red;
    std::wstring green;
    std::wstring blue;
};

struct DyeProfileInputResult {
    DyeProfileInputStatus status = DyeProfileInputStatus::MissingName;
    std::optional<DyeProfile> dye;
    std::wstring message;

    bool IsValid() const { return dye.has_value(); }
};

class DyeProfileFormParser {
public:
    static DyeProfileInputResult Parse(const DyeProfileInput& input);
};
