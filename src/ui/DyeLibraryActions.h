#pragma once

#include "../imaging/Fluorescence.h"
#include "DyeProfileFormParser.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

enum class DyeLibraryActionStatus {
    InvalidInput,
    NoSelection,
    Saved,
    Deleted
};

struct DyeLibraryActionResult {
    DyeLibraryActionStatus status = DyeLibraryActionStatus::InvalidInput;
    bool changed = false;
    std::optional<std::size_t> selected_index;
    std::optional<DyeProfile> dye;
    std::wstring message;
};

class DyeLibraryActions {
public:
    static DyeLibraryActionResult Save(
        std::vector<DyeProfile>& dyes,
        const DyeProfileInput& input);

    static DyeLibraryActionResult DeleteSelected(
        std::vector<DyeProfile>& dyes,
        int selection);
};
