#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class PointCloudUnit {
    Unknown,
    Micrometers,
    Millimeters,
    Meters
};

struct PointCloudPoint {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    std::uint8_t r = 205;
    std::uint8_t g = 221;
    std::uint8_t b = 238;
    bool has_color = false;
};

struct PointCloudBounds {
    double min_x = 0.0;
    double max_x = 0.0;
    double min_y = 0.0;
    double max_y = 0.0;
    double min_z = 0.0;
    double max_z = 0.0;
    bool valid = false;

    double Width() const { return valid ? max_x - min_x : 0.0; }
    double Depth() const { return valid ? max_y - min_y : 0.0; }
    double Height() const { return valid ? max_z - min_z : 0.0; }
};

struct PointCloudCentroid {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct PointCloud {
    std::vector<PointCloudPoint> points;
    PointCloudBounds bounds;
    PointCloudUnit unit = PointCloudUnit::Unknown;
    std::wstring name;
    std::wstring source_path;
    std::wstring format_name;
    std::size_t organized_width = 0;
    std::size_t organized_height = 0;
    bool texture_available = false;

    bool Empty() const { return points.empty(); }
    std::size_t Size() const { return points.size(); }
    bool IsOrganized() const
    {
        return organized_width > 1 && organized_height > 1 &&
            organized_width <= points.size() &&
            organized_height == points.size() / organized_width &&
            points.size() % organized_width == 0;
    }
    bool HasTextureSurface() const { return texture_available && IsOrganized(); }
    void ClearOrganization()
    {
        organized_width = 0;
        organized_height = 0;
        texture_available = false;
    }
    void RecalculateBounds();
    PointCloudCentroid Centroid() const;
};

const wchar_t* PointCloudUnitLabel(PointCloudUnit unit);
double PointCloudUnitToMeters(PointCloudUnit unit);
