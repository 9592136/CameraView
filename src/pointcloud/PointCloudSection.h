#pragma once

#include "PointCloud.h"

#include <cstddef>
#include <vector>

struct PointCloudSectionSample {
    double distance = 0.0;
    double height = 0.0;
    std::size_t source_count = 0;
};

struct PointCloudSectionProfile {
    std::vector<PointCloudSectionSample> samples;
    double width = 0.0;
    double signed_step_height = 0.0;
    double groove_depth = 0.0;
    double minimum_height = 0.0;
    double maximum_height = 0.0;
    bool valid = false;
};

class PointCloudSectionAnalyzer final {
public:
    static PointCloudSectionProfile Analyze(
        const PointCloud& cloud,
        const std::vector<std::size_t>& indices,
        std::size_t requested_bins = 160);
};
