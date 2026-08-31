#include "PointCloudSection.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

double Median(std::vector<double> values)
{
    if (values.empty()) return 0.0;
    const std::size_t middle = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + middle, values.end());
    const double upper = values[middle];
    if (values.size() % 2 != 0) return upper;
    std::nth_element(values.begin(), values.begin() + middle - 1, values.begin() + middle);
    return (values[middle - 1] + upper) * 0.5;
}

} // namespace

PointCloudSectionProfile PointCloudSectionAnalyzer::Analyze(
    const PointCloud& cloud,
    const std::vector<std::size_t>& indices,
    std::size_t requested_bins)
{
    PointCloudSectionProfile profile;
    if (indices.size() < 4 || cloud.Empty()) return profile;
    double center_x = 0.0;
    double center_y = 0.0;
    std::size_t valid_count = 0;
    for (std::size_t index : indices) {
        if (index >= cloud.points.size()) continue;
        center_x += cloud.points[index].x;
        center_y += cloud.points[index].y;
        ++valid_count;
    }
    if (valid_count < 4) return profile;
    center_x /= valid_count;
    center_y /= valid_count;
    double xx = 0.0, xy = 0.0, yy = 0.0;
    for (std::size_t index : indices) {
        if (index >= cloud.points.size()) continue;
        const double x = cloud.points[index].x - center_x;
        const double y = cloud.points[index].y - center_y;
        xx += x * x;
        xy += x * y;
        yy += y * y;
    }
    const double angle = 0.5 * std::atan2(2.0 * xy, xx - yy);
    const double axis_x = std::cos(angle);
    const double axis_y = std::sin(angle);
    struct Projection { double coordinate; double height; };
    std::vector<Projection> projections;
    projections.reserve(valid_count);
    double minimum_coordinate = std::numeric_limits<double>::infinity();
    double maximum_coordinate = -std::numeric_limits<double>::infinity();
    for (std::size_t index : indices) {
        if (index >= cloud.points.size()) continue;
        const PointCloudPoint& point = cloud.points[index];
        const double coordinate = (point.x - center_x) * axis_x +
            (point.y - center_y) * axis_y;
        projections.push_back({coordinate, point.z});
        minimum_coordinate = std::min(minimum_coordinate, coordinate);
        maximum_coordinate = std::max(maximum_coordinate, coordinate);
    }
    profile.width = maximum_coordinate - minimum_coordinate;
    if (profile.width <= 1e-15) return profile;
    const std::size_t bins = std::clamp<std::size_t>(requested_bins, 8, 2048);
    struct Bin { double sum = 0.0; std::size_t count = 0; };
    std::vector<Bin> aggregate(bins);
    for (const Projection& point : projections) {
        const double ratio = (point.coordinate - minimum_coordinate) / profile.width;
        const std::size_t bin = std::min(bins - 1,
            static_cast<std::size_t>(std::floor(ratio * bins)));
        aggregate[bin].sum += point.height;
        ++aggregate[bin].count;
    }
    for (std::size_t bin = 0; bin < bins; ++bin) {
        if (aggregate[bin].count == 0) continue;
        profile.samples.push_back({
            profile.width * (bin + 0.5) / bins,
            aggregate[bin].sum / aggregate[bin].count,
            aggregate[bin].count});
    }
    if (profile.samples.size() < 4) return profile;
    profile.minimum_height = std::numeric_limits<double>::infinity();
    profile.maximum_height = -std::numeric_limits<double>::infinity();
    for (const auto& sample : profile.samples) {
        profile.minimum_height = std::min(profile.minimum_height, sample.height);
        profile.maximum_height = std::max(profile.maximum_height, sample.height);
    }
    const std::size_t edge_count = std::max<std::size_t>(1, profile.samples.size() / 5);
    std::vector<double> first_heights;
    std::vector<double> last_heights;
    for (std::size_t index = 0; index < edge_count; ++index) {
        first_heights.push_back(profile.samples[index].height);
        last_heights.push_back(profile.samples[profile.samples.size() - 1 - index].height);
    }
    const double first_level = Median(first_heights);
    const double last_level = Median(last_heights);
    profile.signed_step_height = last_level - first_level;
    // A groove must fall below both edge plateaus. Using the lower edge level
    // avoids reporting a simple one-sided step as a false groove.
    const double groove_reference = std::min(first_level, last_level);
    profile.groove_depth = std::max(0.0, groove_reference - profile.minimum_height);
    profile.valid = true;
    return profile;
}
