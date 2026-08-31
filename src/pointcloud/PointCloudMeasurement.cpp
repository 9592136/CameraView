#include "PointCloudMeasurement.h"

#include <algorithm>
#include <cmath>

PointCloudVector3 PointCloudMeasurement::Delta(
    const PointCloudPoint& first,
    const PointCloudPoint& second)
{
    return {second.x - first.x, second.y - first.y, second.z - first.z};
}
double PointCloudMeasurement::Distance(
    const PointCloudPoint& first,
    const PointCloudPoint& second)
{
    const PointCloudVector3 delta = Delta(first, second);
    return std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
}

double PointCloudMeasurement::HeightDifference(
    const PointCloudPoint& first,
    const PointCloudPoint& second)
{
    return second.z - first.z;
}

double PointCloudMeasurement::AngleDegrees(
    const PointCloudPoint& first,
    const PointCloudPoint& vertex,
    const PointCloudPoint& third)
{
    const PointCloudVector3 left = Delta(vertex, first);
    const PointCloudVector3 right = Delta(vertex, third);
    const double left_length = std::sqrt(
        left.x * left.x + left.y * left.y + left.z * left.z);
    const double right_length = std::sqrt(
        right.x * right.x + right.y * right.y + right.z * right.z);
    if (left_length <= 1e-15 || right_length <= 1e-15) return 0.0;
    const double cosine = std::clamp(
        (left.x * right.x + left.y * right.y + left.z * right.z) /
            (left_length * right_length),
        -1.0, 1.0);
    return std::acos(cosine) * 180.0 / 3.14159265358979323846;
}

double PointCloudMeasurement::PointToPlaneDistance(
    const PointCloudPoint& point,
    const PointCloudPlane& plane)
{
    return std::abs(plane.SignedDistance(point));
}
