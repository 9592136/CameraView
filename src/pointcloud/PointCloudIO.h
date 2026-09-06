#pragma once

#include "PointCloud.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

class PointCloudIO final {
public:
    using LoadProgress = std::function<bool(std::uint64_t bytesRead, std::uint64_t totalBytes)>;

    static bool Load(
        const std::filesystem::path& path,
        PointCloud& cloud,
        std::wstring& error,
        PointCloudUnit unit = PointCloudUnit::Unknown,
        const LoadProgress& progress = {});
    static bool SavePly(
        const std::filesystem::path& path,
        const PointCloud& cloud,
        std::wstring& error);
    static bool SaveXyz(
        const std::filesystem::path& path,
        const PointCloud& cloud,
        std::wstring& error);
};
