#include "DyeLibrary.h"

#include <algorithm>

std::vector<DyeProfile> DyeLibrary::DefaultDyes()
{
    return {
        DyeProfile{L"DAPI", 358.0, 461.0, RgbColor{80, 120, 255}},
        DyeProfile{L"FITC", 495.0, 519.0, RgbColor{80, 255, 80}},
        DyeProfile{L"TRITC", 557.0, 576.0, RgbColor{255, 96, 32}},
        DyeProfile{L"Cy5", 650.0, 670.0, RgbColor{255, 64, 170}}
    };
}

DyeProfile DyeLibrary::FallbackDye()
{
    return DyeProfile{L"Channel", 0.0, 0.0, RgbColor{255, 255, 255}};
}

const DyeProfile* DyeLibrary::FindByName(const std::vector<DyeProfile>& dyes, const std::wstring& name)
{
    for (const DyeProfile& dye : dyes) {
        if (dye.name == name) {
            return &dye;
        }
    }
    return nullptr;
}

std::optional<std::size_t> DyeLibrary::IndexAtSelection(int selection, std::size_t dye_count)
{
    if (selection < 0 || static_cast<std::size_t>(selection) >= dye_count) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(selection);
}

DyeLibraryUpdateResult DyeLibrary::UpsertByName(std::vector<DyeProfile>& dyes, const DyeProfile& dye)
{
    for (std::size_t index = 0; index < dyes.size(); ++index) {
        if (dyes[index].name == dye.name) {
            dyes[index] = dye;
            return DyeLibraryUpdateResult{index, true};
        }
    }

    dyes.push_back(dye);
    return DyeLibraryUpdateResult{dyes.size() - 1, false};
}

DyeLibraryDeleteResult DyeLibrary::DeleteAt(std::vector<DyeProfile>& dyes, std::size_t index)
{
    if (index >= dyes.size()) {
        return DyeLibraryDeleteResult();
    }

    DyeLibraryDeleteResult result;
    result.deleted = true;
    result.removed = dyes[index];
    dyes.erase(dyes.begin() + static_cast<std::ptrdiff_t>(index));
    if (!dyes.empty()) {
        result.next_index = std::min(index, dyes.size() - 1);
    }
    return result;
}
