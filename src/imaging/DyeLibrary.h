#pragma once

#include "Fluorescence.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

struct DyeLibraryUpdateResult {
    std::size_t index = 0;
    bool updated_existing = false;
};

struct DyeLibraryDeleteResult {
    bool deleted = false;
    DyeProfile removed;
    std::optional<std::size_t> next_index;
};

class DyeLibrary {
public:
    static std::vector<DyeProfile> DefaultDyes();
    static DyeProfile FallbackDye();
    static const DyeProfile* FindByName(const std::vector<DyeProfile>& dyes, const std::wstring& name);
    static std::optional<std::size_t> IndexAtSelection(int selection, std::size_t dye_count);
    static DyeLibraryUpdateResult UpsertByName(std::vector<DyeProfile>& dyes, const DyeProfile& dye);
    static DyeLibraryDeleteResult DeleteAt(std::vector<DyeProfile>& dyes, std::size_t index);
};
