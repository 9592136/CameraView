#pragma once

#include <cmath>

struct ImagePoint {
    double x = 0.0;
    double y = 0.0;
};

inline double DistancePixels(ImagePoint first, ImagePoint second)
{
    const double dx = second.x - first.x;
    const double dy = second.y - first.y;
    return std::sqrt(dx * dx + dy * dy);
}
