#pragma once

#include "PointCloud.h"
#include "PointCloudProcessor.h"

#include <array>
#include <string>
#include <vector>

struct PointCloudLine3D {
    PointCloudPoint point;
    std::array<double, 3> direction{1.0, 0.0, 0.0};
    bool valid = false;
};

struct PointCloudLineIntersection {
    PointCloudPoint point;
    PointCloudPoint point_on_first;
    PointCloudPoint point_on_second;
    double separation = 0.0;
    bool valid = false;
    bool intersects = false;
};

struct PointCloudToleranceLimits {
    double flatness = 0.0;
    double cylindricity = 0.0;
    double circularity = 0.0;
    double warpage = 0.0;
    double profile = 0.0;
};

struct PointCloudToleranceMetric {
    std::wstring name;
    double measured = 0.0;
    double tolerance = 0.0;
    bool valid = false;
    bool passed = false;
    std::wstring method;
};

struct PointCloudToleranceReport {
    std::vector<PointCloudToleranceMetric> metrics;
    PointCloudPlane reference_plane;
    PointCloudLine3D cylinder_axis;
    double fitted_circle_radius = 0.0;
    bool all_valid = false;
    bool all_passed = false;
};

class PointCloudMetrology final {
public:
    static PointCloudPlane PlaneFromPoints(
        const PointCloudPoint& first,
        const PointCloudPoint& second,
        const PointCloudPoint& third);
    static PointCloudLine3D LineFromPoints(
        const PointCloudPoint& first,
        const PointCloudPoint& second);
    static PointCloudLineIntersection IntersectLines(
        const PointCloudLine3D& first,
        const PointCloudLine3D& second,
        double tolerance = 1e-6);
    static double PlaneAngleDegrees(
        const PointCloudPlane& first,
        const PointCloudPlane& second);
    static PointCloudToleranceReport EvaluateTolerances(
        const PointCloud& cloud,
        const PointCloudToleranceLimits& limits);
};
