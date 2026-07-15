#pragma once

#include "../domain/ImageFrame.h"
#include "../domain/Measurement.h"

#include <filesystem>
#include <string>
#include <vector>

class ImageExporter {
public:
    static bool LoadBmp(
        const std::filesystem::path& path,
        ImageFrame& frame,
        std::wstring& error);

    static bool SaveBmp(
        const std::filesystem::path& path,
        const ImageFrame& frame,
        const std::vector<LengthMeasurement>& measurements,
        std::wstring& error);

    static bool SaveBmp(
        const std::filesystem::path& path,
        const ImageFrame& frame,
        const std::vector<LengthMeasurement>& length_measurements,
        const std::vector<AngleMeasurement>& angle_measurements,
        const std::vector<RectangleAreaMeasurement>& rectangle_measurements,
        const std::vector<PolygonAreaMeasurement>& polygon_measurements,
        std::wstring& error);
};
