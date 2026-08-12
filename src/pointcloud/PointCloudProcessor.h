#pragma once

#include "PointCloud.h"

#include <cstddef>
#include <vector>

struct PointCloudPlane {
    double nx = 0.0;
    double ny = 0.0;
    double nz = 1.0;
    double d = 0.0;
    double rms = 0.0;
    bool valid = false;

    double SignedDistance(const PointCloudPoint& point) const;
};

class PointCloudProcessor final {
public:
    static PointCloud VoxelDownsample(const PointCloud& cloud, double voxel_size);
    static PointCloud Crop(const PointCloud& cloud, const PointCloudBounds& bounds);
    static PointCloud SelectIndices(
        const PointCloud& cloud,
        const std::vector<std::size_t>& indices,
        bool keep_selected);
    static PointCloud RemoveRadiusOutliers(
        const PointCloud& cloud,
        double radius,
        std::size_t minimum_neighbors);
    static PointCloudPlane FitPlane(const PointCloud& cloud);
    static PointCloud LevelToPlane(const PointCloud& cloud, const PointCloudPlane& plane);
};
