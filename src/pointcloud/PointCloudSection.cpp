#include "PointCloudSection.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEpsilon = 1e-12;

struct Vector3 { double x = 0.0, y = 0.0, z = 0.0; };

Vector3 subtract(const PointCloudPoint& left, const PointCloudPoint& right)
{
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

PointCloudPoint subtract(const PointCloudPoint& point, const Vector3& offset)
{
    return {point.x - offset.x, point.y - offset.y, point.z - offset.z};
}

Vector3 multiply(const Vector3& value, double factor)
{
    return {value.x * factor, value.y * factor, value.z * factor};
}

double dot(const Vector3& left, const Vector3& right)
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

Vector3 cross(const Vector3& left, const Vector3& right)
{
    return {left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x};
}

double length(const Vector3& value) { return std::sqrt(dot(value, value)); }

Vector3 normalized(const Vector3& value)
{
    const double value_length = length(value);
    return value_length > kEpsilon ? multiply(value, 1.0 / value_length) : Vector3{};
}

double median(std::vector<double> values)
{
    if (values.empty()) return 0.0;
    const std::size_t middle = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + middle, values.end());
    const double upper = values[middle];
    if ((values.size() & 1U) != 0U) return upper;
    std::nth_element(values.begin(), values.begin() + middle - 1, values.begin() + middle);
    return (values[middle - 1] + upper) * 0.5;
}

double mad(const std::vector<double>& values, double center)
{
    std::vector<double> deviations;
    deviations.reserve(values.size());
    for (double value : values) deviations.push_back(std::abs(value - center));
    return median(std::move(deviations));
}

double estimateSpacing(const PointCloud& cloud)
{
    if (!cloud.bounds.valid || cloud.points.empty()) return 0.0;
    const double area = std::max({
        cloud.bounds.Width() * cloud.bounds.Depth(),
        cloud.bounds.Width() * cloud.bounds.Height(),
        cloud.bounds.Depth() * cloud.bounds.Height(), kEpsilon});
    return std::sqrt(area / static_cast<double>(cloud.points.size()));
}

double rangeMedian(const std::vector<PointCloudSectionSample>& samples,
    std::size_t first, std::size_t last)
{
    std::vector<double> values;
    if (samples.empty()) return 0.0;
    last = std::min(last, samples.size() - 1);
    for (std::size_t index = first; index <= last; ++index) {
        if (samples[index].valid) values.push_back(samples[index].height);
    }
    return median(std::move(values));
}

std::vector<double> medianFilter(
    const std::vector<PointCloudSectionSample>& samples, std::size_t window)
{
    if ((window & 1U) == 0U) ++window;
    window = std::max<std::size_t>(1, window);
    const std::size_t radius = window / 2;
    std::vector<double> result(samples.size(), std::numeric_limits<double>::quiet_NaN());
    for (std::size_t index = 0; index < samples.size(); ++index) {
        if (!samples[index].valid) continue;
        std::vector<double> values;
        const std::size_t first = index > radius ? index - radius : 0;
        const std::size_t last = std::min(samples.size() - 1, index + radius);
        for (std::size_t neighbor = first; neighbor <= last; ++neighbor) {
            if (samples[neighbor].valid) values.push_back(samples[neighbor].raw_height);
        }
        if (!values.empty()) result[index] = median(std::move(values));
    }
    return result;
}

std::vector<double> robustLocalFit(const std::vector<PointCloudSectionSample>& samples,
    const std::vector<double>& source, std::size_t window)
{
    if ((window & 1U) == 0U) ++window;
    window = std::max<std::size_t>(3, window);
    const std::size_t radius = window / 2;
    std::vector<double> result(source.size(), std::numeric_limits<double>::quiet_NaN());
    for (std::size_t center = 0; center < source.size(); ++center) {
        if (!std::isfinite(source[center])) continue;
        const std::size_t first = center > radius ? center - radius : 0;
        const std::size_t last = std::min(source.size() - 1, center + radius);
        double intercept = source[center], slope = 0.0;
        for (int iteration = 0; iteration < 3; ++iteration) {
            std::vector<double> residuals;
            for (std::size_t index = first; index <= last; ++index) {
                if (!std::isfinite(source[index])) continue;
                const double x = samples[index].distance - samples[center].distance;
                residuals.push_back(source[index] - intercept - slope * x);
            }
            const double residual_center = median(residuals);
            const double scale = std::max(1.4826 * mad(residuals, residual_center), kEpsilon);
            double sw = 0.0, sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
            for (std::size_t index = first; index <= last; ++index) {
                if (!std::isfinite(source[index])) continue;
                const double x = samples[index].distance - samples[center].distance;
                const double position = radius > 0
                    ? std::abs(static_cast<double>(index) - static_cast<double>(center)) /
                        static_cast<double>(radius + 1) : 0.0;
                const double tricube = std::pow(std::max(0.0,
                    1.0 - std::pow(position, 3.0)), 3.0);
                const double residual = source[index] - intercept - slope * x;
                const double huber = std::abs(residual) <= 1.5 * scale
                    ? 1.0 : 1.5 * scale / std::abs(residual);
                const double weight = tricube * huber;
                sw += weight; sx += weight * x; sy += weight * source[index];
                sxx += weight * x * x; sxy += weight * x * source[index];
            }
            const double determinant = sw * sxx - sx * sx;
            if (sw <= kEpsilon) break;
            if (std::abs(determinant) <= kEpsilon) {
                intercept = sy / sw; slope = 0.0;
            } else {
                intercept = (sy * sxx - sx * sxy) / determinant;
                slope = (sw * sxy - sx * sy) / determinant;
            }
        }
        result[center] = intercept;
    }
    return result;
}

double heightAt(const PointCloudSectionProfile& profile, double distance)
{
    const auto upper = std::lower_bound(profile.samples.begin(), profile.samples.end(), distance,
        [](const auto& sample, double value) { return sample.distance < value; });
    if (upper == profile.samples.begin()) return upper->height;
    if (upper == profile.samples.end()) return profile.samples.back().height;
    const auto& right = *upper;
    const auto& left = *(upper - 1);
    if (!left.valid) return right.height;
    if (!right.valid) return left.height;
    const double span = right.distance - left.distance;
    const double ratio = span > kEpsilon ? (distance - left.distance) / span : 0.0;
    return left.height + (right.height - left.height) * ratio;
}

} // namespace

PointCloudSectionProfile PointCloudSectionAnalyzer::Analyze(
    const PointCloud& cloud,
    const PointCloudSectionDefinition& requested_definition,
    const PointCloudSectionAnalysisOptions& options,
    const std::atomic_bool* cancel)
{
    PointCloudSectionProfile profile;
    profile.definition = requested_definition;
    if (cloud.Empty()) { profile.failure_reason = L"Point cloud is empty."; return profile; }

    Vector3 normal{0.0, 0.0, 1.0};
    if (profile.definition.reference == PointCloudSectionReference::ReferencePlane) {
        if (!profile.definition.reference_plane.valid) {
            profile.failure_reason = L"The reference plane is invalid."; return profile;
        }
        normal = normalized({profile.definition.reference_plane.nx,
            profile.definition.reference_plane.ny, profile.definition.reference_plane.nz});
    }
    if (length(normal) <= kEpsilon) {
        profile.failure_reason = L"The section reference normal is degenerate."; return profile;
    }
    const auto height_of = [&](const PointCloudPoint& point) {
        return profile.definition.reference == PointCloudSectionReference::ReferencePlane
            ? profile.definition.reference_plane.SignedDistance(point) : point.z;
    };
    const auto project = [&](const PointCloudPoint& point) {
        const double offset = profile.definition.reference == PointCloudSectionReference::ReferencePlane
            ? profile.definition.reference_plane.SignedDistance(point) : point.z;
        return subtract(point, multiply(normal, offset));
    };
    const PointCloudPoint start = project(profile.definition.start);
    const PointCloudPoint end = project(profile.definition.end);
    const Vector3 direction = subtract(end, start);
    profile.width = length(direction);
    if (profile.width <= kEpsilon) {
        profile.failure_reason = L"The section endpoints are coincident in the reference plane.";
        return profile;
    }
    const Vector3 longitudinal = multiply(direction, 1.0 / profile.width);
    const Vector3 transverse = normalized(cross(normal, longitudinal));
    const double nominal_spacing = std::max(estimateSpacing(cloud), profile.width / 2000.0);
    if (!(profile.definition.band_width > kEpsilon))
        profile.definition.band_width = std::max(nominal_spacing * 8.0, profile.width / 250.0);
    if (!(profile.definition.sample_spacing > kEpsilon))
        profile.definition.sample_spacing = std::max(nominal_spacing * 2.0, profile.width / 400.0);
    const double half_width = profile.definition.band_width * 0.5;
    const std::size_t stride = options.preview
        ? std::max<std::size_t>(1, cloud.points.size() / 120000) : 1;
    for (std::size_t index = 0; index < cloud.points.size(); index += stride) {
        if (cancel && (index & 4095U) == 0U && cancel->load(std::memory_order_relaxed)) {
            profile.cancelled = true;
            profile.failure_reason = L"Section analysis was cancelled.";
            return profile;
        }
        const auto& point = cloud.points[index];
        const Vector3 relative = subtract(point, start);
        const double distance = dot(relative, longitudinal);
        const double offset = dot(relative, transverse);
        if (distance >= 0.0 && distance <= profile.width && std::abs(offset) <= half_width)
            profile.raw_points.push_back({distance, height_of(point), offset, index});
    }
    if (profile.raw_points.size() < 4) {
        profile.failure_reason = L"The section band contains too few points."; return profile;
    }
    std::sort(profile.raw_points.begin(), profile.raw_points.end(),
        [](const auto& left, const auto& right) { return left.distance < right.distance; });

    const std::size_t bins = std::clamp<std::size_t>(
        static_cast<std::size_t>(std::ceil(profile.width / profile.definition.sample_spacing)),
        8, std::clamp<std::size_t>(options.maximum_bins, 8, 8192));
    profile.definition.sample_spacing = profile.width / static_cast<double>(bins);
    std::vector<std::vector<double>> heights(bins);
    for (const auto& point : profile.raw_points) {
        const std::size_t bin = std::min(bins - 1,
            static_cast<std::size_t>(point.distance / profile.width * bins));
        heights[bin].push_back(point.height);
    }
    profile.samples.resize(bins);
    for (std::size_t bin = 0; bin < bins; ++bin) {
        auto& sample = profile.samples[bin];
        sample.distance = profile.width * (static_cast<double>(bin) + 0.5) / bins;
        sample.source_count = heights[bin].size();
        if (heights[bin].empty()) continue;
        sample.raw_height = median(heights[bin]);
        sample.mad = mad(heights[bin], sample.raw_height);
        sample.filtered_height = sample.height = sample.raw_height;
        sample.confidence = std::min(1.0, static_cast<double>(sample.source_count) / 6.0) /
            (1.0 + sample.mad / std::max(nominal_spacing, kEpsilon));
        sample.valid = true;
    }
    for (std::size_t first = 0; first < profile.samples.size();) {
        if (profile.samples[first].valid) { ++first; continue; }
        std::size_t end_gap = first;
        while (end_gap < profile.samples.size() && !profile.samples[end_gap].valid) ++end_gap;
        const std::size_t gap = end_gap - first;
        if (first > 0 && end_gap < profile.samples.size() && gap <= options.maximum_gap_bins) {
            const double left = profile.samples[first - 1].raw_height;
            const double right = profile.samples[end_gap].raw_height;
            for (std::size_t offset = 0; offset < gap; ++offset) {
                auto& sample = profile.samples[first + offset];
                const double ratio = static_cast<double>(offset + 1) / static_cast<double>(gap + 1);
                sample.raw_height = sample.filtered_height = sample.height = left + (right - left) * ratio;
                sample.interpolated = true; sample.confidence = 0.25; sample.valid = true;
            }
        }
        first = end_gap;
    }
    const auto medians = medianFilter(profile.samples, options.median_window);
    const auto fitted = robustLocalFit(profile.samples, medians, options.smoothing_window);
    profile.minimum_height = std::numeric_limits<double>::infinity();
    profile.maximum_height = -std::numeric_limits<double>::infinity();
    for (std::size_t index = 0; index < profile.samples.size(); ++index) {
        auto& sample = profile.samples[index];
        if (!sample.valid || !std::isfinite(fitted[index])) continue;
        sample.filtered_height = sample.height = fitted[index];
        profile.minimum_height = std::min(profile.minimum_height, sample.height);
        profile.maximum_height = std::max(profile.maximum_height, sample.height);
    }
    if (!std::isfinite(profile.minimum_height)) {
        profile.failure_reason = L"No valid profile samples were produced."; return profile;
    }
    profile.total_peak_to_valley = profile.maximum_height - profile.minimum_height;
    const std::size_t edge_count = std::max<std::size_t>(1, profile.samples.size() / 5);
    const double first_level = rangeMedian(profile.samples, 0, edge_count - 1);
    const double last_level = rangeMedian(profile.samples,
        profile.samples.size() - edge_count, profile.samples.size() - 1);
    profile.signed_step_height = last_level - first_level;
    profile.groove_depth = std::max(0.0, std::min(first_level, last_level) - profile.minimum_height);
    profile.valid = true;
    DetectFeatures(profile, options);
    if (profile.definition.reference_detached)
        profile.warnings.push_back(L"The reference model was removed; its plane snapshot is being used.");
    return profile;
}

PointCloudSectionProfile PointCloudSectionAnalyzer::Analyze(
    const PointCloud& cloud, const std::vector<std::size_t>& indices, std::size_t requested_bins)
{
    PointCloud subset;
    subset.unit = cloud.unit;
    double center_x = 0.0, center_y = 0.0;
    for (std::size_t index : indices) {
        if (index >= cloud.points.size()) continue;
        subset.points.push_back(cloud.points[index]);
        center_x += cloud.points[index].x; center_y += cloud.points[index].y;
    }
    if (subset.points.size() < 4) return {};
    center_x /= subset.points.size(); center_y /= subset.points.size();
    double xx = 0.0, xy = 0.0, yy = 0.0;
    for (const auto& point : subset.points) {
        const double x = point.x - center_x, y = point.y - center_y;
        xx += x * x; xy += x * y; yy += y * y;
    }
    const double angle = 0.5 * std::atan2(2.0 * xy, xx - yy);
    const double axis_x = std::cos(angle), axis_y = std::sin(angle);
    double minimum = std::numeric_limits<double>::infinity();
    double maximum = -std::numeric_limits<double>::infinity();
    double transverse_maximum = 0.0, average_z = 0.0;
    for (const auto& point : subset.points) {
        const double x = point.x - center_x, y = point.y - center_y;
        minimum = std::min(minimum, x * axis_x + y * axis_y);
        maximum = std::max(maximum, x * axis_x + y * axis_y);
        transverse_maximum = std::max(transverse_maximum, std::abs(-x * axis_y + y * axis_x));
        average_z += point.z;
    }
    average_z /= subset.points.size(); subset.RecalculateBounds();
    PointCloudSectionDefinition definition;
    definition.start = {center_x + minimum * axis_x, center_y + minimum * axis_y, average_z};
    definition.end = {center_x + maximum * axis_x, center_y + maximum * axis_y, average_z};
    definition.band_width = std::max(transverse_maximum * 2.0 + kEpsilon, kEpsilon * 10.0);
    definition.sample_spacing = (maximum - minimum) /
        static_cast<double>(std::clamp<std::size_t>(requested_bins, 8, 2048));
    PointCloudSectionAnalysisOptions options;
    options.maximum_bins = std::clamp<std::size_t>(requested_bins, 8, 2048);
    options.median_window = 1; options.smoothing_window = 3;
    return Analyze(subset, definition, options);
}

void PointCloudSectionAnalyzer::DetectFeatures(
    PointCloudSectionProfile& profile, const PointCloudSectionAnalysisOptions& options)
{
    profile.features.clear();
    if (!profile.valid || profile.samples.size() < 5) return;
    std::vector<double> derivatives;
    for (std::size_t index = 1; index < profile.samples.size(); ++index) {
        const auto& left = profile.samples[index - 1];
        const auto& right = profile.samples[index];
        if (left.valid && right.valid) derivatives.push_back((right.height - left.height) /
            std::max(right.distance - left.distance, kEpsilon));
    }
    if (derivatives.empty()) return;
    const double derivative_center = median(derivatives);
    const double derivative_noise = 1.4826 * mad(derivatives, derivative_center);
    const double threshold = std::max(options.detection_sensitivity * derivative_noise,
        profile.total_peak_to_valley / std::max(profile.width, kEpsilon) * 0.08);
    struct Plateau { std::size_t first, last; double level, confidence; };
    std::vector<Plateau> plateaus;
    std::size_t run_start = 0; bool in_run = false;
    auto append_plateau = [&](std::size_t first, std::size_t last) {
        if (last + 1 - first < options.minimum_plateau_bins) return;
        double confidence = 0.0;
        for (std::size_t sample = first; sample <= last; ++sample)
            confidence += profile.samples[sample].confidence;
        plateaus.push_back({first, last, rangeMedian(profile.samples, first, last),
            confidence / static_cast<double>(last + 1 - first)});
    };
    for (std::size_t index = 1; index < profile.samples.size(); ++index) {
        const auto& left = profile.samples[index - 1];
        const auto& right = profile.samples[index];
        const double slope = left.valid && right.valid
            ? std::abs((right.height - left.height) /
                std::max(right.distance - left.distance, kEpsilon))
            : std::numeric_limits<double>::infinity();
        if (slope <= threshold) {
            if (!in_run) { run_start = index - 1; in_run = true; }
        } else if (in_run) {
            append_plateau(run_start, index - 1); in_run = false;
        }
    }
    if (in_run) append_plateau(run_start, profile.samples.size() - 1);
    const double height_noise = std::max(derivative_noise * profile.definition.sample_spacing,
        profile.total_peak_to_valley * 0.01);
    std::uint64_t next_id = 1;
    for (std::size_t index = 1; index < plateaus.size(); ++index) {
        const auto& left = plateaus[index - 1]; const auto& right = plateaus[index];
        const double height = right.level - left.level;
        if (std::abs(height) <= std::max(3.0 * height_noise, kEpsilon)) continue;
        PointCloudSectionFeature feature;
        feature.id = next_id++; feature.type = PointCloudSectionFeatureType::Step;
        feature.start_distance = profile.samples[left.last].distance;
        feature.end_distance = profile.samples[right.first].distance;
        feature.left_plateau_start = profile.samples[left.first].distance;
        feature.left_plateau_end = profile.samples[left.last].distance;
        feature.right_plateau_start = profile.samples[right.first].distance;
        feature.right_plateau_end = profile.samples[right.last].distance;
        feature.signed_height = height; feature.depth_or_height = std::abs(height);
        feature.opening_width = feature.end_distance - feature.start_distance;
        feature.half_height_width = feature.opening_width;
        feature.slope_angle_degrees = std::atan2(height,
            std::max(feature.opening_width, kEpsilon)) * 180.0 / kPi;
        feature.confidence = std::min(left.confidence, right.confidence);
        profile.features.push_back(feature);
    }
    for (std::size_t index = 1; index + 1 < plateaus.size(); ++index) {
        const auto& left = plateaus[index - 1]; const auto& middle = plateaus[index];
        const auto& right = plateaus[index + 1];
        const double baseline = (left.level + right.level) * 0.5;
        const double excursion = middle.level - baseline;
        if (std::abs(excursion) <= std::max(3.0 * height_noise, kEpsilon)) continue;
        PointCloudSectionFeature feature;
        feature.id = next_id++;
        feature.type = excursion < 0.0 ? PointCloudSectionFeatureType::Groove
                                      : PointCloudSectionFeatureType::Peak;
        feature.start_distance = profile.samples[left.last].distance;
        feature.end_distance = profile.samples[right.first].distance;
        feature.left_plateau_start = profile.samples[left.first].distance;
        feature.left_plateau_end = profile.samples[left.last].distance;
        feature.right_plateau_start = profile.samples[right.first].distance;
        feature.right_plateau_end = profile.samples[right.last].distance;
        feature.signed_height = excursion; feature.depth_or_height = std::abs(excursion);
        feature.opening_width = feature.end_distance - feature.start_distance;
        const double half_level = baseline + excursion * 0.5;
        bool half_started = false; double half_first = 0.0, half_last = 0.0, area = 0.0;
        for (std::size_t sample = left.last; sample <= right.first; ++sample) {
            if (!profile.samples[sample].valid) continue;
            const double deviation = profile.samples[sample].height - baseline;
            const bool beyond_half = excursion < 0.0
                ? profile.samples[sample].height <= half_level
                : profile.samples[sample].height >= half_level;
            if (beyond_half && !half_started) { half_first = profile.samples[sample].distance; half_started = true; }
            if (beyond_half) half_last = profile.samples[sample].distance;
            if (sample > left.last) {
                const double previous = profile.samples[sample - 1].height - baseline;
                area += (std::abs(previous) + std::abs(deviation)) * 0.5 *
                    (profile.samples[sample].distance - profile.samples[sample - 1].distance);
            }
        }
        feature.half_height_width = half_started ? half_last - half_first : 0.0;
        feature.area = area;
        feature.confidence = std::min({left.confidence, middle.confidence, right.confidence});
        profile.features.push_back(feature);
    }
}

PointCloudSectionCursorMeasurement PointCloudSectionAnalyzer::MeasureCursors(
    const PointCloudSectionProfile& profile, double first_distance, double second_distance)
{
    PointCloudSectionCursorMeasurement result;
    if (!profile.valid || profile.samples.empty()) return result;
    result.first_distance = std::clamp(first_distance, 0.0, profile.width);
    result.second_distance = std::clamp(second_distance, 0.0, profile.width);
    result.first_height = heightAt(profile, result.first_distance);
    result.second_height = heightAt(profile, result.second_distance);
    result.distance_difference = result.second_distance - result.first_distance;
    result.height_difference = result.second_height - result.first_height;
    result.spatial_distance = std::hypot(result.distance_difference, result.height_difference);
    result.slope_angle_degrees = std::atan2(result.height_difference,
        std::abs(result.distance_difference) > kEpsilon ? result.distance_difference : kEpsilon)
        * 180.0 / kPi;
    result.valid = true;
    return result;
}
