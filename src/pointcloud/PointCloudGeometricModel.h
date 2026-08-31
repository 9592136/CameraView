#pragma once

#include "PointCloud.h"
#include "PointCloudProcessor.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class PointCloudGeometricModelType {
    Plane,
    Sphere,
    Cylinder
};

enum class PointCloudFitScope {
    WholeCloud,
    Selection
};

enum class PointCloudCylinderAxisConstraint {
    Free,
    X,
    Y,
    Z
};

struct PointCloudFitOptions {
    PointCloudFitScope scope = PointCloudFitScope::WholeCloud;
    PointCloudCylinderAxisConstraint cylinder_axis = PointCloudCylinderAxisConstraint::Free;
    double inlier_threshold = 0.0;
    double minimum_radius = 0.0;
    double maximum_radius = 0.0;
    std::size_t ransac_iterations = 384;
    std::uint32_t random_seed = 0x43414d45U;
};

struct PointCloudFitQuality {
    std::size_t sample_count = 0;
    std::size_t inlier_count = 0;
    double inlier_ratio = 0.0;
    double mean = 0.0;
    double standard_deviation = 0.0;
    double rms = 0.0;
    double minimum = 0.0;
    double maximum = 0.0;
    bool valid = false;
};

struct PointCloudSphereParameters {
    PointCloudPoint center;
    double radius = 0.0;
    bool valid = false;
};

struct PointCloudCylinderParameters {
    PointCloudPoint axis_point;
    std::array<double, 3> axis_direction{0.0, 0.0, 1.0};
    double radius = 0.0;
    double axial_minimum = 0.0;
    double axial_maximum = 0.0;
    bool valid = false;
};

struct PointCloudGeometricModel {
    std::uint64_t id = 0;
    std::wstring name;
    PointCloudGeometricModelType type = PointCloudGeometricModelType::Plane;
    PointCloudFitScope source_scope = PointCloudFitScope::WholeCloud;
    bool visible = true;
    PointCloudPlane plane;
    PointCloudSphereParameters sphere;
    PointCloudCylinderParameters cylinder;
    PointCloudFitQuality quality;
    std::vector<std::size_t> source_indices;
    std::vector<std::size_t> inlier_indices;
    std::vector<double> residuals;
};

struct PointCloudFitResult {
    PointCloudGeometricModel model;
    std::wstring error;
    bool valid = false;
};

class PointCloudGeometricFitter final {
public:
    static PointCloudFitResult FitPlane(
        const PointCloud& cloud,
        const std::vector<std::size_t>& indices = {},
        const PointCloudFitOptions& options = {});
    static PointCloudFitResult FitSphere(
        const PointCloud& cloud,
        const std::vector<std::size_t>& indices = {},
        const PointCloudFitOptions& options = {});
    static PointCloudFitResult FitCylinder(
        const PointCloud& cloud,
        const std::vector<std::size_t>& indices = {},
        const PointCloudFitOptions& options = {});
    static double Residual(
        const PointCloudGeometricModel& model,
        const PointCloudPoint& point);
};
