#pragma once

#include "PointCloud.h"
#include "PointCloudProcessor.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class PointCloudSectionReference { WorldZ, ReferencePlane };
enum class PointCloudSectionFeatureType { Step, Groove, Peak };

struct PointCloudSectionDefinition {
    std::uint64_t id = 0;
    std::wstring name;
    bool visible = true;
    bool include_in_report = true;
    PointCloudPoint start;
    PointCloudPoint end;
    double band_width = 0.0;
    double sample_spacing = 0.0;
    PointCloudSectionReference reference = PointCloudSectionReference::WorldZ;
    std::uint64_t reference_model_id = 0;
    PointCloudPlane reference_plane;
    bool reference_detached = false;
    std::uint32_t color_rgb = 0x4da6ff;
};

struct PointCloudSectionAnalysisOptions {
    std::size_t median_window = 5;
    std::size_t smoothing_window = 11;
    std::size_t maximum_gap_bins = 3;
    double detection_sensitivity = 3.5;
    std::size_t minimum_plateau_bins = 5;
    std::size_t maximum_bins = 2048;
    bool preview = false;
};

struct PointCloudSectionRawPoint {
    double distance = 0.0;
    double height = 0.0;
    double transverse_offset = 0.0;
    std::size_t source_index = 0;
};

struct PointCloudSectionSample {
    double distance = 0.0;
    // Robust display value retained under the old name for source compatibility.
    double height = 0.0;
    double raw_height = 0.0;
    double filtered_height = 0.0;
    double mad = 0.0;
    std::size_t source_count = 0;
    double confidence = 0.0;
    bool interpolated = false;
    bool valid = false;
};

struct PointCloudSectionFeature {
    std::uint64_t id = 0;
    PointCloudSectionFeatureType type = PointCloudSectionFeatureType::Step;
    double start_distance = 0.0;
    double end_distance = 0.0;
    double left_plateau_start = 0.0;
    double left_plateau_end = 0.0;
    double right_plateau_start = 0.0;
    double right_plateau_end = 0.0;
    double signed_height = 0.0;
    double depth_or_height = 0.0;
    double opening_width = 0.0;
    double half_height_width = 0.0;
    double area = 0.0;
    double slope_angle_degrees = 0.0;
    double confidence = 0.0;
    bool automatic = true;
};

struct PointCloudSectionCursorMeasurement {
    double first_distance = 0.0;
    double second_distance = 0.0;
    double first_height = 0.0;
    double second_height = 0.0;
    double distance_difference = 0.0;
    double height_difference = 0.0;
    double spatial_distance = 0.0;
    double slope_angle_degrees = 0.0;
    bool valid = false;
};

struct PointCloudSectionProfile {
    PointCloudSectionDefinition definition;
    std::vector<PointCloudSectionRawPoint> raw_points;
    std::vector<PointCloudSectionSample> samples;
    std::vector<PointCloudSectionFeature> features;
    std::vector<std::wstring> warnings;
    std::wstring failure_reason;
    double width = 0.0;
    double signed_step_height = 0.0;
    double groove_depth = 0.0;
    double minimum_height = 0.0;
    double maximum_height = 0.0;
    double total_peak_to_valley = 0.0;
    bool cancelled = false;
    bool valid = false;
};

class PointCloudSectionAnalyzer final {
public:
    static PointCloudSectionProfile Analyze(
        const PointCloud& cloud,
        const PointCloudSectionDefinition& definition,
        const PointCloudSectionAnalysisOptions& options = {},
        const std::atomic_bool* cancel = nullptr);

    // Compatibility entry point for callers that still provide a preselected strip.
    static PointCloudSectionProfile Analyze(
        const PointCloud& cloud,
        const std::vector<std::size_t>& indices,
        std::size_t requested_bins = 160);

    static void DetectFeatures(
        PointCloudSectionProfile& profile,
        const PointCloudSectionAnalysisOptions& options = {});
    static PointCloudSectionCursorMeasurement MeasureCursors(
        const PointCloudSectionProfile& profile,
        double first_distance,
        double second_distance);
};
