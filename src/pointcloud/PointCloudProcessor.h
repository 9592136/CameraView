#pragma once

#include "PointCloud.h"

#include <cstddef>
#include <vector>
#include <string>

struct PointCloudPlane {
    double nx = 0.0;
    double ny = 0.0;
    double nz = 1.0;
    double d = 0.0;
    double rms = 0.0;
    bool valid = false;

    double SignedDistance(const PointCloudPoint& point) const;
};

struct PointCloudDenoiseOptions {
    double neighbor_radius = 0.0;
    std::size_t minimum_neighbors = 4;
    double spike_sigma = 3.5;
    double minimum_height_deviation = 0.0;
    double smoothing_strength = 0.25;
    int smoothing_iterations = 1;
};

struct PointCloudDenoiseReport {
    std::size_t removed_isolated = 0;
    std::size_t removed_spikes = 0;
    std::size_t smoothed_points = 0;
};

struct PointCloudHoleRepairOptions {
    double grid_spacing = 0.0;
    std::size_t maximum_hole_cells = 64;
    int search_radius_cells = 5;
    int smoothing_iterations = 2;
    std::size_t maximum_grid_cells = 4000000;
};

struct PointCloudHoleRepairReport {
    bool applicable = true;
    std::size_t detected_holes = 0;
    std::size_t filled_holes = 0;
    std::size_t filled_points = 0;
    std::size_t skipped_large_holes = 0;
    std::wstring message;
};

class PointCloudProcessor final {
public:
    static PointCloud VoxelDownsample(const PointCloud& cloud, double voxel_size);
    static PointCloud SelectIndices(
        const PointCloud& cloud,
        const std::vector<std::size_t>& indices,
        bool keep_selected);
    static PointCloud RemoveRadiusOutliers(
        const PointCloud& cloud,
        double radius,
        std::size_t minimum_neighbors);
    static double EstimateNominalSpacing(const PointCloud& cloud);
    static PointCloud SmartDenoise(
        const PointCloud& cloud,
        const PointCloudDenoiseOptions& options,
        PointCloudDenoiseReport* report = nullptr);
    static PointCloud RepairHoles(
        const PointCloud& cloud,
        const PointCloudHoleRepairOptions& options,
        PointCloudHoleRepairReport* report = nullptr);
    static PointCloudPlane FitPlane(const PointCloud& cloud);
    static PointCloud LevelToPlane(const PointCloud& cloud, const PointCloudPlane& plane);
};
