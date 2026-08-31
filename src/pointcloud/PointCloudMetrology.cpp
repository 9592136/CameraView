#include "PointCloudMetrology.h"
#include "PointCloudGeometricModel.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr double kPi = 3.14159265358979323846;

std::array<double, 3> Difference(const PointCloudPoint& a, const PointCloudPoint& b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

double Dot(const std::array<double, 3>& a, const std::array<double, 3>& b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

double Length(const std::array<double, 3>& value)
{
    return std::sqrt(Dot(value, value));
}

std::array<double, 3> Cross(
    const std::array<double, 3>& a,
    const std::array<double, 3>& b)
{
    return {a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0]};
}

bool Normalize(std::array<double, 3>& value)
{
    const double length = Length(value);
    if (length <= 1e-15) return false;
    for (double& component : value) component /= length;
    return true;
}

bool Solve3x3(double matrix[3][4], std::array<double, 3>& solution)
{
    for (int column = 0; column < 3; ++column) {
        int pivot = column;
        for (int row = column + 1; row < 3; ++row) {
            if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column])) pivot = row;
        }
        if (std::abs(matrix[pivot][column]) < 1e-14) return false;
        if (pivot != column) {
            for (int value = column; value < 4; ++value) {
                std::swap(matrix[pivot][value], matrix[column][value]);
            }
        }
        const double scale = matrix[column][column];
        for (int value = column; value < 4; ++value) matrix[column][value] /= scale;
        for (int row = 0; row < 3; ++row) {
            if (row == column) continue;
            const double factor = matrix[row][column];
            for (int value = column; value < 4; ++value) {
                matrix[row][value] -= factor * matrix[column][value];
            }
        }
    }
    solution = {matrix[0][3], matrix[1][3], matrix[2][3]};
    return true;
}

struct AxisCandidate {
    std::array<double, 3> direction{};
    double radial_range = std::numeric_limits<double>::infinity();
    double mean_radius = 0.0;
    bool valid = false;
};

std::array<std::array<double, 3>, 3> CovarianceAxes(const PointCloud& cloud)
{
    std::array<std::array<double, 3>, 3> covariance{};
    std::array<std::array<double, 3>, 3> axes{{
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0}}};
    const PointCloudCentroid center = cloud.Centroid();
    for (const PointCloudPoint& point : cloud.points) {
        const double delta[3]{point.x - center.x, point.y - center.y, point.z - center.z};
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                covariance[row][column] += delta[row] * delta[column];
            }
        }
    }
    for (int iteration = 0; iteration < 40; ++iteration) {
        int p = 0;
        int q = 1;
        double largest = std::abs(covariance[0][1]);
        for (int row = 0; row < 3; ++row) {
            for (int column = row + 1; column < 3; ++column) {
                if (std::abs(covariance[row][column]) > largest) {
                    largest = std::abs(covariance[row][column]);
                    p = row;
                    q = column;
                }
            }
        }
        if (largest < 1e-13) break;
        const double angle = 0.5 * std::atan2(2.0 * covariance[p][q],
            covariance[q][q] - covariance[p][p]);
        const double sine = std::sin(angle);
        const double cosine = std::cos(angle);
        for (int index = 0; index < 3; ++index) {
            const double left = covariance[index][p];
            const double right = covariance[index][q];
            covariance[index][p] = cosine * left - sine * right;
            covariance[index][q] = sine * left + cosine * right;
        }
        for (int index = 0; index < 3; ++index) {
            const double top = covariance[p][index];
            const double bottom = covariance[q][index];
            covariance[p][index] = cosine * top - sine * bottom;
            covariance[q][index] = sine * top + cosine * bottom;
        }
        covariance[p][q] = covariance[q][p] = 0.0;
        for (int row = 0; row < 3; ++row) {
            const double first = axes[row][p];
            const double second = axes[row][q];
            axes[row][p] = cosine * first - sine * second;
            axes[row][q] = sine * first + cosine * second;
        }
    }
    return {{{axes[0][0], axes[1][0], axes[2][0]},
        {axes[0][1], axes[1][1], axes[2][1]},
        {axes[0][2], axes[1][2], axes[2][2]}}};
}

AxisCandidate EvaluateAxis(
    const PointCloud& cloud,
    const PointCloudPoint& center,
    std::array<double, 3> direction)
{
    AxisCandidate candidate;
    if (!Normalize(direction) || cloud.points.size() < 6) return candidate;
    double minimum = std::numeric_limits<double>::infinity();
    double maximum = 0.0;
    double sum = 0.0;
    for (const PointCloudPoint& point : cloud.points) {
        const std::array<double, 3> delta = Difference(point, center);
        const double axial = Dot(delta, direction);
        const std::array<double, 3> radial{
            delta[0] - axial * direction[0],
            delta[1] - axial * direction[1],
            delta[2] - axial * direction[2]};
        const double radius = Length(radial);
        minimum = std::min(minimum, radius);
        maximum = std::max(maximum, radius);
        sum += radius;
    }
    candidate.direction = direction;
    candidate.radial_range = maximum - minimum;
    candidate.mean_radius = sum / cloud.points.size();
    candidate.valid = candidate.mean_radius > 1e-12 && std::isfinite(candidate.radial_range);
    return candidate;
}

PointCloudToleranceMetric Metric(
    const wchar_t* name,
    double value,
    double tolerance,
    bool valid,
    const wchar_t* method)
{
    return {name, value, tolerance, valid, valid && tolerance > 0.0 && value <= tolerance, method};
}

PointCloudLine3D PrincipalAxis(const PointCloud& cloud)
{
    PointCloudLine3D axis;
    if (cloud.points.size() < 6) return axis;
    const PointCloudCentroid center = cloud.Centroid();
    axis.point = {center.x, center.y, center.z};
    AxisCandidate best;
    for (const auto& candidate_axis : CovarianceAxes(cloud)) {
        const AxisCandidate candidate = EvaluateAxis(cloud, axis.point, candidate_axis);
        const double score = candidate.valid
            ? candidate.radial_range / candidate.mean_radius
            : std::numeric_limits<double>::infinity();
        const double best_score = best.valid
            ? best.radial_range / best.mean_radius
            : std::numeric_limits<double>::infinity();
        if (score < best_score) best = candidate;
    }
    if (best.valid) {
        axis.direction = best.direction;
        axis.valid = true;
    }
    return axis;
}

} // namespace

PointCloudPlane PointCloudMetrology::PlaneFromPoints(
    const PointCloudPoint& first,
    const PointCloudPoint& second,
    const PointCloudPoint& third)
{
    PointCloudPlane plane;
    const std::array<double, 3> first_vector = Difference(second, first);
    const std::array<double, 3> second_vector = Difference(third, first);
    std::array<double, 3> normal{
        first_vector[1] * second_vector[2] - first_vector[2] * second_vector[1],
        first_vector[2] * second_vector[0] - first_vector[0] * second_vector[2],
        first_vector[0] * second_vector[1] - first_vector[1] * second_vector[0]};
    if (!Normalize(normal)) return plane;
    plane.nx = normal[0];
    plane.ny = normal[1];
    plane.nz = normal[2];
    plane.d = -(plane.nx * first.x + plane.ny * first.y + plane.nz * first.z);
    plane.valid = true;
    return plane;
}

PointCloudLine3D PointCloudMetrology::LineFromPoints(
    const PointCloudPoint& first,
    const PointCloudPoint& second)
{
    PointCloudLine3D line;
    line.point = first;
    line.direction = Difference(second, first);
    line.valid = Normalize(line.direction);
    return line;
}

PointCloudLineIntersection PointCloudMetrology::IntersectLines(
    const PointCloudLine3D& first,
    const PointCloudLine3D& second,
    double tolerance)
{
    PointCloudLineIntersection result;
    if (!first.valid || !second.valid) return result;
    const std::array<double, 3> offset = Difference(first.point, second.point);
    const double a = Dot(first.direction, first.direction);
    const double b = Dot(first.direction, second.direction);
    const double c = Dot(second.direction, second.direction);
    const double d = Dot(first.direction, offset);
    const double e = Dot(second.direction, offset);
    const double denominator = a * c - b * b;
    if (std::abs(denominator) < 1e-14) return result;
    const double first_parameter = (b * e - c * d) / denominator;
    const double second_parameter = (a * e - b * d) / denominator;
    result.point_on_first = {
        first.point.x + first_parameter * first.direction[0],
        first.point.y + first_parameter * first.direction[1],
        first.point.z + first_parameter * first.direction[2]};
    result.point_on_second = {
        second.point.x + second_parameter * second.direction[0],
        second.point.y + second_parameter * second.direction[1],
        second.point.z + second_parameter * second.direction[2]};
    result.separation = Length(Difference(result.point_on_first, result.point_on_second));
    result.point = {
        (result.point_on_first.x + result.point_on_second.x) * 0.5,
        (result.point_on_first.y + result.point_on_second.y) * 0.5,
        (result.point_on_first.z + result.point_on_second.z) * 0.5};
    result.valid = true;
    result.intersects = result.separation <= std::max(tolerance, 0.0);
    return result;
}

double PointCloudMetrology::PlaneAngleDegrees(
    const PointCloudPlane& first,
    const PointCloudPlane& second)
{
    if (!first.valid || !second.valid) return std::numeric_limits<double>::quiet_NaN();
    const double dot = std::clamp(std::abs(
        first.nx * second.nx + first.ny * second.ny + first.nz * second.nz), 0.0, 1.0);
    return std::acos(dot) * 180.0 / kPi;
}

PointCloudToleranceReport PointCloudMetrology::EvaluateTolerances(
    const PointCloud& cloud,
    const PointCloudToleranceLimits& limits)
{
    PointCloudToleranceReport report;
    report.reference_plane = PointCloudProcessor::FitPlane(cloud);
    double flatness = 0.0;
    double warpage = 0.0;
    double profile = 0.0;
    bool plane_valid = report.reference_plane.valid;
    if (plane_valid) {
        double minimum = std::numeric_limits<double>::infinity();
        double maximum = -std::numeric_limits<double>::infinity();
        double maximum_absolute = 0.0;
        for (const PointCloudPoint& point : cloud.points) {
            const double deviation = report.reference_plane.SignedDistance(point);
            minimum = std::min(minimum, deviation);
            maximum = std::max(maximum, deviation);
            maximum_absolute = std::max(maximum_absolute, std::abs(deviation));
        }
        flatness = maximum - minimum;
        warpage = maximum_absolute;
        profile = 2.0 * maximum_absolute;
    }

    const PointCloudFitResult cylinder_fit = PointCloudGeometricFitter::FitCylinder(cloud);
    if (cylinder_fit.valid) {
        report.cylinder_axis.point = cylinder_fit.model.cylinder.axis_point;
        report.cylinder_axis.direction = cylinder_fit.model.cylinder.axis_direction;
        report.cylinder_axis.valid = true;
        report.fitted_circle_radius = cylinder_fit.model.cylinder.radius;
    } else {
        report.cylinder_axis = PrincipalAxis(cloud);
    }
    bool circle_valid = cylinder_fit.valid;
    double circle_form = 0.0;
    if (cylinder_fit.valid) {
        circle_form = cylinder_fit.model.quality.maximum - cylinder_fit.model.quality.minimum;
    } else if (report.cylinder_axis.valid && cloud.points.size() >= 3) {
        const std::array<double, 3> reference =
            std::abs(report.cylinder_axis.direction[2]) < 0.9
            ? std::array<double, 3>{0.0, 0.0, 1.0}
            : std::array<double, 3>{1.0, 0.0, 0.0};
        std::array<double, 3> first_axis = Cross(report.cylinder_axis.direction, reference);
        circle_valid = Normalize(first_axis);
        const std::array<double, 3> second_axis =
            Cross(report.cylinder_axis.direction, first_axis);
        double matrix[3][4]{};
        for (const PointCloudPoint& point : cloud.points) {
            const std::array<double, 3> delta = Difference(point, report.cylinder_axis.point);
            const double x = Dot(delta, first_axis);
            const double y = Dot(delta, second_axis);
            const double row[3]{2.0 * x, 2.0 * y, 1.0};
            const double right = x * x + y * y;
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) matrix[i][j] += row[i] * row[j];
                matrix[i][3] += row[i] * right;
            }
        }
        std::array<double, 3> circle{};
        circle_valid = Solve3x3(matrix, circle);
        if (circle_valid) {
            double minimum = std::numeric_limits<double>::infinity();
            double maximum = 0.0;
            double sum = 0.0;
            for (const PointCloudPoint& point : cloud.points) {
                const std::array<double, 3> delta = Difference(point, report.cylinder_axis.point);
                const double x = Dot(delta, first_axis);
                const double y = Dot(delta, second_axis);
                const double radius = std::hypot(x - circle[0], y - circle[1]);
                minimum = std::min(minimum, radius);
                maximum = std::max(maximum, radius);
                sum += radius;
            }
            report.fitted_circle_radius = sum / cloud.points.size();
            circle_form = maximum - minimum;
        }
    }

    double cylindricity = 0.0;
    if (cylinder_fit.valid) {
        cylindricity = cylinder_fit.model.quality.maximum - cylinder_fit.model.quality.minimum;
    } else if (report.cylinder_axis.valid) {
        double minimum = std::numeric_limits<double>::infinity();
        double maximum = 0.0;
        for (const PointCloudPoint& point : cloud.points) {
            const std::array<double, 3> delta = Difference(point, report.cylinder_axis.point);
            const double axial = Dot(delta, report.cylinder_axis.direction);
            std::array<double, 3> radial{
                delta[0] - axial * report.cylinder_axis.direction[0],
                delta[1] - axial * report.cylinder_axis.direction[1],
                delta[2] - axial * report.cylinder_axis.direction[2]};
            const double radius = Length(radial);
            minimum = std::min(minimum, radius);
            maximum = std::max(maximum, radius);
        }
        cylindricity = maximum - minimum;
    }

    report.metrics.push_back(Metric(L"Flatness", flatness, limits.flatness,
        plane_valid, L"least-squares plane peak-to-valley"));
    report.metrics.push_back(Metric(L"Cylindricity", cylindricity, limits.cylindricity,
        report.cylinder_axis.valid, L"robust unified-cylinder radial peak-to-valley"));
    report.metrics.push_back(Metric(L"Circularity", circle_form, limits.circularity,
        circle_valid, L"robust unified-cylinder radial peak-to-valley"));
    report.metrics.push_back(Metric(L"Warpage", warpage, limits.warpage,
        plane_valid, L"maximum absolute deviation from best-fit plane"));
    report.metrics.push_back(Metric(L"Profile", profile, limits.profile,
        plane_valid, L"bilateral profile zone about best-fit plane"));
    report.all_valid = std::all_of(report.metrics.begin(), report.metrics.end(),
        [](const auto& metric) { return metric.valid; });
    report.all_passed = report.all_valid && std::all_of(report.metrics.begin(), report.metrics.end(),
        [](const auto& metric) { return metric.passed; });
    return report;
}
