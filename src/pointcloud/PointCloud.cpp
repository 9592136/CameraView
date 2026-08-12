#include "PointCloud.h"

#include <algorithm>
#include <limits>

void PointCloud::RecalculateBounds()
{
    bounds = {};
    if (points.empty()) return;
    bounds.min_x = bounds.min_y = bounds.min_z = std::numeric_limits<double>::max();
    bounds.max_x = bounds.max_y = bounds.max_z = std::numeric_limits<double>::lowest();
    for (const PointCloudPoint& point : points) {
        bounds.min_x = std::min(bounds.min_x, point.x);
        bounds.max_x = std::max(bounds.max_x, point.x);
        bounds.min_y = std::min(bounds.min_y, point.y);
        bounds.max_y = std::max(bounds.max_y, point.y);
        bounds.min_z = std::min(bounds.min_z, point.z);
        bounds.max_z = std::max(bounds.max_z, point.z);
    }
    bounds.valid = true;
}
PointCloudCentroid PointCloud::Centroid() const
{
    PointCloudCentroid centroid;
    if (points.empty()) return centroid;
    for (const PointCloudPoint& point : points) {
        centroid.x += point.x;
        centroid.y += point.y;
        centroid.z += point.z;
    }
    const double count = static_cast<double>(points.size());
    centroid.x /= count;
    centroid.y /= count;
    centroid.z /= count;
    return centroid;
}

const wchar_t* PointCloudUnitLabel(PointCloudUnit unit)
{
    switch (unit) {
    case PointCloudUnit::Micrometers: return L"\u00b5m";
    case PointCloudUnit::Millimeters: return L"mm";
    case PointCloudUnit::Meters: return L"m";
    case PointCloudUnit::Unknown:
    default: return L"unit";
    }
}

double PointCloudUnitToMeters(PointCloudUnit unit)
{
    switch (unit) {
    case PointCloudUnit::Micrometers: return 1e-6;
    case PointCloudUnit::Millimeters: return 1e-3;
    case PointCloudUnit::Meters: return 1.0;
    case PointCloudUnit::Unknown:
    default: return 1.0;
    }
}
