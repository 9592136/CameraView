#pragma once

#include "PointCloud.h"
#include "PointCloudGeometricModel.h"
#include "PointCloudProcessor.h"

#include <cstddef>
#include <vector>

struct PointCloudDeviationBin {
    double minimum = 0.0;
    double maximum = 0.0;
    double center = 0.0;
    std::size_t count = 0;
    double gaussian_expected_count = 0.0;
};

struct PointCloudDeviationDistribution {
    std::vector<double> deviations;
    std::vector<PointCloudDeviationBin> bins;
    PointCloudPlane reference_plane;
    PointCloudGeometricModelType reference_type = PointCloudGeometricModelType::Plane;
    std::wstring reference_name;
    double minimum = 0.0;
    double maximum = 0.0;
    double mean = 0.0;
    double standard_deviation = 0.0;
    double rms = 0.0;
    double skewness = 0.0;
    double excess_kurtosis = 0.0;
    double within_one_sigma_percent = 0.0;
    double within_two_sigma_percent = 0.0;
    double within_three_sigma_percent = 0.0;
    bool valid = false;
};

class PointCloudDeviationAnalyzer final {
public:
    static PointCloudDeviationDistribution Analyze(
        const PointCloud& cloud,
        const PointCloudPlane& plane,
        std::size_t requested_bins = 0);
    static PointCloudDeviationDistribution Analyze(
        const PointCloud& cloud,
        const PointCloudGeometricModel& model,
        const std::vector<std::size_t>& indices = {},
        std::size_t requested_bins = 0);
};
