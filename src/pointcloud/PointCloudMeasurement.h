#pragma once

#include "PointCloud.h"
#include "PointCloudProcessor.h"

struct PointCloudVector3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};
class PointCloudMeasurement final {
public:
    static double Distance(const PointCloudPoint& first, const PointCloudPoint& second);
    static PointCloudVector3 Delta(
        const PointCloudPoint& first,
        const PointCloudPoint& second);
    static double HeightDifference(
        const PointCloudPoint& first,
        const PointCloudPoint& second);
    static double AngleDegrees(
        const PointCloudPoint& first,
        const PointCloudPoint& vertex,
        const PointCloudPoint& third);
    static double PointToPlaneDistance(
        const PointCloudPoint& point,
        const PointCloudPlane& plane);
};
