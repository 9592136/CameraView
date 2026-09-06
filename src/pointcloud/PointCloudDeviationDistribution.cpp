#include "PointCloudDeviationDistribution.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace {

constexpr double kPi = 3.14159265358979323846;

PointCloudDeviationDistribution AnalyzeResiduals(
    std::vector<double> deviations,
    std::size_t requested_bins)
{
    PointCloudDeviationDistribution result;
    result.deviations = std::move(deviations);
    if (result.deviations.size() < 3) return result;
    double sum = 0.0;
    double squared_sum = 0.0;
    result.minimum = std::numeric_limits<double>::infinity();
    result.maximum = -std::numeric_limits<double>::infinity();
    for (double deviation : result.deviations) {
        sum += deviation;
        squared_sum += deviation * deviation;
        result.minimum = std::min(result.minimum, deviation);
        result.maximum = std::max(result.maximum, deviation);
    }
    const std::size_t count = result.deviations.size();
    result.mean = sum / count;
    result.rms = std::sqrt(squared_sum / count);
    double centered_squared = 0.0;
    double centered_cubed = 0.0;
    double centered_fourth = 0.0;
    for (double deviation : result.deviations) {
        const double centered = deviation - result.mean;
        const double square = centered * centered;
        centered_squared += square;
        centered_cubed += square * centered;
        centered_fourth += square * square;
    }
    result.standard_deviation = std::sqrt(centered_squared / count);
    const double sigma = result.standard_deviation;
    if (sigma > 1e-15) {
        const double sigma_squared = sigma * sigma;
        result.skewness = centered_cubed / count / (sigma_squared * sigma);
        result.excess_kurtosis = centered_fourth / count / (sigma_squared * sigma_squared) - 3.0;
        std::size_t within_one = 0, within_two = 0, within_three = 0;
        for (double deviation : result.deviations) {
            const double normalized = std::abs(deviation - result.mean) / sigma;
            if (normalized <= 1.0) ++within_one;
            if (normalized <= 2.0) ++within_two;
            if (normalized <= 3.0) ++within_three;
        }
        const double percent = 100.0 / count;
        result.within_one_sigma_percent = within_one * percent;
        result.within_two_sigma_percent = within_two * percent;
        result.within_three_sigma_percent = within_three * percent;
    } else {
        result.within_one_sigma_percent = 100.0;
        result.within_two_sigma_percent = 100.0;
        result.within_three_sigma_percent = 100.0;
    }
    const std::size_t automatic_bins = static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(count))));
    const std::size_t bin_count = std::clamp<std::size_t>(requested_bins > 0 ? requested_bins : automatic_bins, 8, 160);
    double range = result.maximum - result.minimum;
    if (range <= 1e-15) {
        range = std::max(std::abs(result.mean) * 0.1, 1e-9);
        result.minimum = result.mean - range * 0.5;
        result.maximum = result.mean + range * 0.5;
    }
    const double bin_width = range / bin_count;
    result.bins.resize(bin_count);
    for (std::size_t index = 0; index < bin_count; ++index) {
        auto& bin = result.bins[index];
        bin.minimum = result.minimum + index * bin_width;
        bin.maximum = index + 1 == bin_count ? result.maximum : bin.minimum + bin_width;
        bin.center = (bin.minimum + bin.maximum) * 0.5;
    }
    for (double deviation : result.deviations) {
        const double ratio = (deviation - result.minimum) / range;
        const std::size_t index = std::min(bin_count - 1,
            static_cast<std::size_t>(std::max(0.0, std::floor(ratio * bin_count))));
        ++result.bins[index].count;
    }
    if (sigma > 1e-15) {
        const double scale = count * bin_width / (sigma * std::sqrt(2.0 * kPi));
        for (auto& bin : result.bins) {
            const double normalized = (bin.center - result.mean) / sigma;
            bin.gaussian_expected_count = scale * std::exp(-0.5 * normalized * normalized);
        }
    } else {
        result.bins[bin_count / 2].gaussian_expected_count = static_cast<double>(count);
    }
    result.valid = true;
    return result;
}

} // namespace

PointCloudDeviationDistribution PointCloudDeviationAnalyzer::Analyze(
    const PointCloud& cloud,
    const PointCloudPlane& plane,
    std::size_t requested_bins)
{
    PointCloudDeviationDistribution metadata;
    metadata.reference_plane = plane;
    if (!plane.valid || cloud.points.size() < 3) return metadata;
    std::vector<double> deviations;
    deviations.reserve(cloud.points.size());
    for (const PointCloudPoint& point : cloud.points) {
        const double deviation = plane.SignedDistance(point);
        if (!std::isfinite(deviation)) continue;
        deviations.push_back(deviation);
    }
    PointCloudDeviationDistribution result = AnalyzeResiduals(std::move(deviations), requested_bins);
    result.reference_plane = plane;
    return result;
}

PointCloudDeviationDistribution PointCloudDeviationAnalyzer::Analyze(
    const PointCloud& cloud,
    const PointCloudGeometricModel& model,
    const std::vector<std::size_t>& requested,
    std::size_t requested_bins)
{
    std::vector<std::size_t> indices = requested;
    if (indices.empty()) {
        indices.resize(cloud.points.size());
        std::iota(indices.begin(), indices.end(), 0);
    }
    std::vector<double> deviations;
    deviations.reserve(indices.size());
    for (std::size_t index : indices) {
        if (index >= cloud.points.size()) continue;
        const double value = PointCloudGeometricFitter::Residual(model, cloud.points[index]);
        if (std::isfinite(value)) deviations.push_back(value);
    }
    PointCloudDeviationDistribution result = AnalyzeResiduals(std::move(deviations), requested_bins);
    result.reference_type = model.type;
    result.reference_name = model.name;
    result.reference_plane = model.plane;
    return result;
}
