#include "PointCloudGeometricModel.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <random>

namespace {

using Vector3 = std::array<double, 3>;

Vector3 difference(const PointCloudPoint& a, const PointCloudPoint& b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

double dot(const Vector3& a, const Vector3& b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

Vector3 cross(const Vector3& a, const Vector3& b)
{
    return {a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0]};
}

double length(const Vector3& value)
{
    return std::sqrt(dot(value, value));
}

bool normalize(Vector3& value)
{
    const double size = length(value);
    if (!(size > 1e-14) || !std::isfinite(size)) return false;
    for (double& component : value) component /= size;
    return true;
}

bool solveLinear(std::vector<std::vector<double>> matrix, std::vector<double>& solution)
{
    const int size = static_cast<int>(matrix.size());
    if (size == 0) return false;
    for (int column = 0; column < size; ++column) {
        int pivot = column;
        for (int row = column + 1; row < size; ++row) {
            if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column])) pivot = row;
        }
        if (std::abs(matrix[pivot][column]) < 1e-12) return false;
        std::swap(matrix[pivot], matrix[column]);
        const double scale = matrix[column][column];
        for (int value = column; value <= size; ++value) matrix[column][value] /= scale;
        for (int row = 0; row < size; ++row) {
            if (row == column) continue;
            const double factor = matrix[row][column];
            for (int value = column; value <= size; ++value) {
                matrix[row][value] -= factor * matrix[column][value];
            }
        }
    }
    solution.resize(size);
    for (int row = 0; row < size; ++row) solution[row] = matrix[row][size];
    return true;
}

std::vector<std::size_t> validIndices(
    const PointCloud& cloud,
    const std::vector<std::size_t>& requested)
{
    if (requested.empty()) {
        std::vector<std::size_t> result(cloud.points.size());
        std::iota(result.begin(), result.end(), 0);
        return result;
    }
    std::vector<std::size_t> result;
    result.reserve(requested.size());
    for (std::size_t index : requested) {
        if (index < cloud.points.size()) result.push_back(index);
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

double median(std::vector<double> values)
{
    if (values.empty()) return 0.0;
    const std::size_t middle = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + middle, values.end());
    double result = values[middle];
    if (values.size() % 2 == 0) {
        std::nth_element(values.begin(), values.begin() + middle - 1, values.end());
        result = (result + values[middle - 1]) * 0.5;
    }
    return result;
}

double robustScale(const std::vector<double>& residuals)
{
    if (residuals.empty()) return 0.0;
    const double center = median(residuals);
    std::vector<double> deviations;
    deviations.reserve(residuals.size());
    for (double value : residuals) deviations.push_back(std::abs(value - center));
    return 1.4826 * median(std::move(deviations));
}

double automaticThreshold(const PointCloud& cloud, const PointCloudFitOptions& options)
{
    if (options.inlier_threshold > 0.0) return options.inlier_threshold;
    const double extent = cloud.bounds.valid
        ? std::max({cloud.bounds.Width(), cloud.bounds.Depth(), cloud.bounds.Height()}) : 0.0;
    return std::max(extent * 0.005, 1e-8);
}

bool radiusAllowed(double radius, const PointCloudFitOptions& options)
{
    return radius > 1e-12 && std::isfinite(radius) &&
        (options.minimum_radius <= 0.0 || radius >= options.minimum_radius) &&
        (options.maximum_radius <= 0.0 || radius <= options.maximum_radius);
}

PointCloudFitQuality qualityFromResiduals(
    const std::vector<double>& residuals,
    double threshold,
    const std::vector<std::size_t>& source,
    std::vector<std::size_t>& inliers)
{
    PointCloudFitQuality quality;
    quality.sample_count = residuals.size();
    if (residuals.empty()) return quality;
    quality.minimum = std::numeric_limits<double>::infinity();
    quality.maximum = -std::numeric_limits<double>::infinity();
    std::vector<double> accepted;
    accepted.reserve(residuals.size());
    for (std::size_t index = 0; index < residuals.size(); ++index) {
        const double value = residuals[index];
        quality.minimum = std::min(quality.minimum, value);
        quality.maximum = std::max(quality.maximum, value);
        if (std::abs(value) <= threshold) {
            inliers.push_back(source[index]);
            accepted.push_back(value);
        }
    }
    if (accepted.empty()) return quality;
    const double sum = std::accumulate(accepted.begin(), accepted.end(), 0.0);
    double squared = 0.0;
    for (double value : accepted) squared += value * value;
    quality.mean = sum / accepted.size();
    quality.rms = std::sqrt(squared / accepted.size());
    double centered = 0.0;
    for (double value : accepted) centered += (value - quality.mean) * (value - quality.mean);
    quality.standard_deviation = std::sqrt(centered / accepted.size());
    quality.inlier_count = inliers.size();
    quality.inlier_ratio = static_cast<double>(quality.inlier_count) / quality.sample_count;
    quality.valid = std::isfinite(quality.rms);
    return quality;
}

bool sphereFromFour(
    const PointCloudPoint& first,
    const PointCloudPoint& second,
    const PointCloudPoint& third,
    const PointCloudPoint& fourth,
    PointCloudSphereParameters& sphere)
{
    const PointCloudPoint points[]{second, third, fourth};
    std::vector<std::vector<double>> matrix(3, std::vector<double>(4));
    const double first_norm = first.x * first.x + first.y * first.y + first.z * first.z;
    for (int row = 0; row < 3; ++row) {
        matrix[row][0] = 2.0 * (points[row].x - first.x);
        matrix[row][1] = 2.0 * (points[row].y - first.y);
        matrix[row][2] = 2.0 * (points[row].z - first.z);
        matrix[row][3] = points[row].x * points[row].x + points[row].y * points[row].y +
            points[row].z * points[row].z - first_norm;
    }
    std::vector<double> solution;
    if (!solveLinear(std::move(matrix), solution)) return false;
    sphere.center = {solution[0], solution[1], solution[2]};
    sphere.radius = length(difference(first, sphere.center));
    sphere.valid = sphere.radius > 1e-12 && std::isfinite(sphere.radius);
    return sphere.valid;
}

double sphereResidual(const PointCloudSphereParameters& sphere, const PointCloudPoint& point)
{
    return length(difference(point, sphere.center)) - sphere.radius;
}

bool refineSphere(
    const PointCloud& cloud,
    const std::vector<std::size_t>& indices,
    PointCloudSphereParameters& sphere,
    const PointCloudFitOptions& options)
{
    for (int iteration = 0; iteration < 24; ++iteration) {
        std::vector<double> residuals;
        residuals.reserve(indices.size());
        for (std::size_t index : indices) residuals.push_back(sphereResidual(sphere, cloud.points[index]));
        const double scale = std::max(robustScale(residuals), automaticThreshold(cloud, options) * 0.2);
        const double huber = std::max(1.345 * scale, 1e-12);
        std::vector<std::vector<double>> normal(4, std::vector<double>(5));
        for (std::size_t sample = 0; sample < indices.size(); ++sample) {
            const PointCloudPoint& point = cloud.points[indices[sample]];
            const Vector3 delta = difference(sphere.center, point);
            const double distance = length(delta);
            if (distance <= 1e-14) continue;
            const double residual = residuals[sample];
            const double weight = std::abs(residual) <= huber ? 1.0 : huber / std::abs(residual);
            const double jacobian[4]{delta[0] / distance, delta[1] / distance,
                delta[2] / distance, -1.0};
            for (int row = 0; row < 4; ++row) {
                for (int column = 0; column < 4; ++column) {
                    normal[row][column] += weight * jacobian[row] * jacobian[column];
                }
                normal[row][4] += -weight * jacobian[row] * residual;
            }
        }
        std::vector<double> delta;
        if (!solveLinear(std::move(normal), delta)) return false;
        sphere.center.x += delta[0];
        sphere.center.y += delta[1];
        sphere.center.z += delta[2];
        sphere.radius += delta[3];
        if (!radiusAllowed(sphere.radius, options)) return false;
        const double change = std::sqrt(delta[0] * delta[0] + delta[1] * delta[1] +
            delta[2] * delta[2] + delta[3] * delta[3]);
        if (change <= std::max(sphere.radius, 1.0) * 1e-10) break;
    }
    sphere.valid = radiusAllowed(sphere.radius, options);
    return sphere.valid;
}

std::array<Vector3, 3> covarianceAxes(
    const PointCloud& cloud,
    const std::vector<std::size_t>& indices)
{
    Vector3 center{};
    for (std::size_t index : indices) {
        center[0] += cloud.points[index].x;
        center[1] += cloud.points[index].y;
        center[2] += cloud.points[index].z;
    }
    for (double& value : center) value /= indices.size();
    double covariance[3][3]{};
    for (std::size_t index : indices) {
        const PointCloudPoint& point = cloud.points[index];
        const double value[3]{point.x - center[0], point.y - center[1], point.z - center[2]};
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) covariance[row][column] += value[row] * value[column];
        }
    }
    double vectors[3][3]{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
    for (int iteration = 0; iteration < 48; ++iteration) {
        int p = 0, q = 1;
        double largest = std::abs(covariance[0][1]);
        for (int row = 0; row < 3; ++row) for (int column = row + 1; column < 3; ++column) {
            if (std::abs(covariance[row][column]) > largest) {
                largest = std::abs(covariance[row][column]); p = row; q = column;
            }
        }
        if (largest < 1e-13) break;
        const double angle = 0.5 * std::atan2(2.0 * covariance[p][q],
            covariance[q][q] - covariance[p][p]);
        const double sine = std::sin(angle), cosine = std::cos(angle);
        for (int index = 0; index < 3; ++index) {
            const double left = covariance[index][p], right = covariance[index][q];
            covariance[index][p] = cosine * left - sine * right;
            covariance[index][q] = sine * left + cosine * right;
        }
        for (int index = 0; index < 3; ++index) {
            const double top = covariance[p][index], bottom = covariance[q][index];
            covariance[p][index] = cosine * top - sine * bottom;
            covariance[q][index] = sine * top + cosine * bottom;
        }
        covariance[p][q] = covariance[q][p] = 0.0;
        for (int row = 0; row < 3; ++row) {
            const double left = vectors[row][p], right = vectors[row][q];
            vectors[row][p] = cosine * left - sine * right;
            vectors[row][q] = sine * left + cosine * right;
        }
    }
    return {{{vectors[0][0], vectors[1][0], vectors[2][0]},
        {vectors[0][1], vectors[1][1], vectors[2][1]},
        {vectors[0][2], vectors[1][2], vectors[2][2]}}};
}

bool fitCircleForAxis(
    const PointCloud& cloud,
    const std::vector<std::size_t>& indices,
    Vector3 axis,
    PointCloudCylinderParameters& cylinder,
    std::vector<double>* output_residuals = nullptr)
{
    if (!normalize(axis) || indices.size() < 6) return false;
    Vector3 reference = std::abs(axis[2]) < 0.8 ? Vector3{0.0, 0.0, 1.0} : Vector3{1.0, 0.0, 0.0};
    Vector3 first = cross(axis, reference);
    if (!normalize(first)) return false;
    Vector3 second = cross(axis, first);
    Vector3 center{};
    for (std::size_t index : indices) {
        center[0] += cloud.points[index].x;
        center[1] += cloud.points[index].y;
        center[2] += cloud.points[index].z;
    }
    for (double& value : center) value /= indices.size();
    double circle_x = 0.0, circle_y = 0.0, radius = 0.0;
    std::vector<double> weights(indices.size(), 1.0);
    for (int iteration = 0; iteration < 10; ++iteration) {
        std::vector<std::vector<double>> normal(3, std::vector<double>(4));
        for (std::size_t sample = 0; sample < indices.size(); ++sample) {
            const Vector3 delta = difference(cloud.points[indices[sample]],
                {center[0], center[1], center[2]});
            const double x = dot(delta, first), y = dot(delta, second);
            const double row[3]{2.0 * x, 2.0 * y, 1.0};
            const double right = x * x + y * y;
            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 3; ++c) normal[r][c] += weights[sample] * row[r] * row[c];
                normal[r][3] += weights[sample] * row[r] * right;
            }
        }
        std::vector<double> solution;
        if (!solveLinear(std::move(normal), solution)) return false;
        circle_x = solution[0]; circle_y = solution[1];
        radius = std::sqrt(std::max(0.0, solution[2] + circle_x * circle_x + circle_y * circle_y));
        if (!(radius > 1e-12)) return false;
        std::vector<double> residuals;
        residuals.reserve(indices.size());
        for (std::size_t index : indices) {
            const Vector3 delta = difference(cloud.points[index], {center[0], center[1], center[2]});
            residuals.push_back(std::hypot(dot(delta, first) - circle_x,
                dot(delta, second) - circle_y) - radius);
        }
        const double scale = std::max(robustScale(residuals), radius * 1e-8);
        const double huber = 1.345 * scale;
        for (std::size_t sample = 0; sample < residuals.size(); ++sample) {
            const double absolute = std::abs(residuals[sample]);
            weights[sample] = absolute <= huber ? 1.0 : huber / absolute;
        }
    }
    cylinder.axis_direction = axis;
    cylinder.axis_point = {center[0] + circle_x * first[0] + circle_y * second[0],
        center[1] + circle_x * first[1] + circle_y * second[1],
        center[2] + circle_x * first[2] + circle_y * second[2]};
    cylinder.radius = radius;
    cylinder.axial_minimum = std::numeric_limits<double>::infinity();
    cylinder.axial_maximum = -std::numeric_limits<double>::infinity();
    std::vector<double> residuals;
    residuals.reserve(indices.size());
    for (std::size_t index : indices) {
        const Vector3 delta = difference(cloud.points[index], cylinder.axis_point);
        const double axial = dot(delta, axis);
        cylinder.axial_minimum = std::min(cylinder.axial_minimum, axial);
        cylinder.axial_maximum = std::max(cylinder.axial_maximum, axial);
        const Vector3 radial{delta[0] - axial * axis[0], delta[1] - axial * axis[1],
            delta[2] - axial * axis[2]};
        residuals.push_back(length(radial) - radius);
    }
    cylinder.valid = true;
    if (output_residuals) *output_residuals = std::move(residuals);
    return true;
}

double cylinderScore(const std::vector<double>& residuals)
{
    std::vector<double> absolute;
    absolute.reserve(residuals.size());
    for (double value : residuals) absolute.push_back(std::abs(value));
    return median(std::move(absolute)) + robustScale(residuals) * 0.25;
}

Vector3 constrainedAxis(PointCloudCylinderAxisConstraint constraint)
{
    if (constraint == PointCloudCylinderAxisConstraint::X) return {1.0, 0.0, 0.0};
    if (constraint == PointCloudCylinderAxisConstraint::Y) return {0.0, 1.0, 0.0};
    return {0.0, 0.0, 1.0};
}

} // namespace

double PointCloudGeometricFitter::Residual(
    const PointCloudGeometricModel& model,
    const PointCloudPoint& point)
{
    if (model.type == PointCloudGeometricModelType::Plane && model.plane.valid) {
        return model.plane.SignedDistance(point);
    }
    if (model.type == PointCloudGeometricModelType::Sphere && model.sphere.valid) {
        return sphereResidual(model.sphere, point);
    }
    if (model.type == PointCloudGeometricModelType::Cylinder && model.cylinder.valid) {
        const Vector3 delta = difference(point, model.cylinder.axis_point);
        const double axial = dot(delta, model.cylinder.axis_direction);
        const Vector3 radial{delta[0] - axial * model.cylinder.axis_direction[0],
            delta[1] - axial * model.cylinder.axis_direction[1],
            delta[2] - axial * model.cylinder.axis_direction[2]};
        return length(radial) - model.cylinder.radius;
    }
    return std::numeric_limits<double>::quiet_NaN();
}

PointCloudFitResult PointCloudGeometricFitter::FitPlane(
    const PointCloud& cloud,
    const std::vector<std::size_t>& requested,
    const PointCloudFitOptions& options)
{
    PointCloudFitResult result;
    result.model.type = PointCloudGeometricModelType::Plane;
    result.model.source_scope = options.scope;
    result.model.source_indices = validIndices(cloud, requested);
    if (result.model.source_indices.size() < 3) {
        result.error = L"At least three selected points are required to fit a plane.";
        return result;
    }
    PointCloud subset;
    subset.unit = cloud.unit;
    subset.points.reserve(result.model.source_indices.size());
    for (std::size_t index : result.model.source_indices) subset.points.push_back(cloud.points[index]);
    subset.RecalculateBounds();
    result.model.plane = PointCloudProcessor::FitPlane(subset);
    if (!result.model.plane.valid) {
        result.error = L"The selected points are degenerate and cannot define a plane.";
        return result;
    }
    result.model.residuals.reserve(result.model.source_indices.size());
    for (std::size_t index : result.model.source_indices) {
        result.model.residuals.push_back(result.model.plane.SignedDistance(cloud.points[index]));
    }
    const double threshold = std::max(automaticThreshold(subset, options),
        std::max(robustScale(result.model.residuals) * 2.5, 1e-10));
    result.model.quality = qualityFromResiduals(result.model.residuals, threshold,
        result.model.source_indices, result.model.inlier_indices);
    result.valid = result.model.quality.valid;
    return result;
}

PointCloudFitResult PointCloudGeometricFitter::FitSphere(
    const PointCloud& cloud,
    const std::vector<std::size_t>& requested,
    const PointCloudFitOptions& options)
{
    PointCloudFitResult result;
    result.model.type = PointCloudGeometricModelType::Sphere;
    result.model.source_scope = options.scope;
    result.model.source_indices = validIndices(cloud, requested);
    if (result.model.source_indices.size() < 4) {
        result.error = L"At least four selected points are required to fit a sphere.";
        return result;
    }
    const double threshold = automaticThreshold(cloud, options);
    std::mt19937 generator(options.random_seed);
    std::uniform_int_distribution<std::size_t> distribution(0, result.model.source_indices.size() - 1);
    PointCloudSphereParameters best;
    std::size_t best_inliers = 0;
    double best_score = std::numeric_limits<double>::infinity();
    const std::size_t iterations = std::max<std::size_t>(options.ransac_iterations, 32);
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
        std::array<std::size_t, 4> sample{};
        for (int item = 0; item < 4; ++item) {
            bool unique = false;
            while (!unique) {
                sample[item] = distribution(generator);
                unique = true;
                for (int previous = 0; previous < item; ++previous) unique &= sample[item] != sample[previous];
            }
        }
        PointCloudSphereParameters candidate;
        if (!sphereFromFour(cloud.points[result.model.source_indices[sample[0]]],
                cloud.points[result.model.source_indices[sample[1]]],
                cloud.points[result.model.source_indices[sample[2]]],
                cloud.points[result.model.source_indices[sample[3]]], candidate) ||
            !radiusAllowed(candidate.radius, options)) continue;
        std::size_t inliers = 0;
        std::vector<double> absolute;
        absolute.reserve(result.model.source_indices.size());
        for (std::size_t index : result.model.source_indices) {
            const double residual = std::abs(sphereResidual(candidate, cloud.points[index]));
            if (residual <= threshold) ++inliers;
            absolute.push_back(residual);
        }
        const double score = median(std::move(absolute));
        if (inliers > best_inliers || (inliers == best_inliers && score < best_score)) {
            best = candidate; best_inliers = inliers; best_score = score;
        }
    }
    if (!best.valid) {
        result.error = L"Sphere fitting failed because no stable four-point model was found.";
        return result;
    }
    std::vector<std::size_t> refine_indices;
    for (std::size_t index : result.model.source_indices) {
        if (std::abs(sphereResidual(best, cloud.points[index])) <= threshold * 2.5) refine_indices.push_back(index);
    }
    if (refine_indices.size() < 4) refine_indices = result.model.source_indices;
    if (!refineSphere(cloud, refine_indices, best, options)) {
        result.error = L"Sphere refinement was unstable or violated the radius constraint.";
        return result;
    }
    result.model.sphere = best;
    result.model.residuals.reserve(result.model.source_indices.size());
    for (std::size_t index : result.model.source_indices) {
        result.model.residuals.push_back(sphereResidual(best, cloud.points[index]));
    }
    const double final_threshold = std::max(threshold, robustScale(result.model.residuals) * 2.5);
    result.model.quality = qualityFromResiduals(result.model.residuals, final_threshold,
        result.model.source_indices, result.model.inlier_indices);
    result.valid = result.model.quality.valid;
    return result;
}

PointCloudFitResult PointCloudGeometricFitter::FitCylinder(
    const PointCloud& cloud,
    const std::vector<std::size_t>& requested,
    const PointCloudFitOptions& options)
{
    PointCloudFitResult result;
    result.model.type = PointCloudGeometricModelType::Cylinder;
    result.model.source_scope = options.scope;
    result.model.source_indices = validIndices(cloud, requested);
    if (result.model.source_indices.size() < 6) {
        result.error = L"At least six selected points are required to fit a cylinder.";
        return result;
    }
    std::vector<Vector3> axes;
    if (options.cylinder_axis == PointCloudCylinderAxisConstraint::Free) {
        const auto covariance = covarianceAxes(cloud, result.model.source_indices);
        axes.assign(covariance.begin(), covariance.end());
    } else {
        axes.push_back(constrainedAxis(options.cylinder_axis));
    }
    PointCloudCylinderParameters best;
    std::vector<double> best_residuals;
    double best_score = std::numeric_limits<double>::infinity();
    for (Vector3 axis : axes) {
        PointCloudCylinderParameters candidate;
        std::vector<double> residuals;
        if (!fitCircleForAxis(cloud, result.model.source_indices, axis, candidate, &residuals) ||
            !radiusAllowed(candidate.radius, options)) continue;
        const double score = cylinderScore(residuals);
        if (score < best_score) {
            best = candidate; best_residuals = std::move(residuals); best_score = score;
        }
    }
    if (!best.valid) {
        result.error = L"Cylinder fitting failed or the fitted radius violates its constraint.";
        return result;
    }
    if (options.cylinder_axis == PointCloudCylinderAxisConstraint::Free) {
        double angular_step = 0.12;
        for (int iteration = 0; iteration < 18; ++iteration) {
            Vector3 reference = std::abs(best.axis_direction[2]) < 0.8
                ? Vector3{0.0, 0.0, 1.0} : Vector3{1.0, 0.0, 0.0};
            Vector3 first = cross(best.axis_direction, reference); normalize(first);
            Vector3 second = cross(best.axis_direction, first); normalize(second);
            bool improved = false;
            for (const Vector3& tangent : {first, second}) for (double sign : {-1.0, 1.0}) {
                Vector3 axis{best.axis_direction[0] + sign * angular_step * tangent[0],
                    best.axis_direction[1] + sign * angular_step * tangent[1],
                    best.axis_direction[2] + sign * angular_step * tangent[2]};
                normalize(axis);
                PointCloudCylinderParameters candidate;
                std::vector<double> residuals;
                if (!fitCircleForAxis(cloud, result.model.source_indices, axis, candidate, &residuals) ||
                    !radiusAllowed(candidate.radius, options)) continue;
                const double score = cylinderScore(residuals);
                if (score + 1e-14 < best_score) {
                    best = candidate; best_residuals = std::move(residuals);
                    best_score = score; improved = true;
                }
            }
            if (!improved) angular_step *= 0.5;
            if (angular_step < 2e-5) break;
        }
    }
    result.model.cylinder = best;
    result.model.residuals = std::move(best_residuals);
    const double threshold = std::max(automaticThreshold(cloud, options),
        robustScale(result.model.residuals) * 2.5);
    result.model.quality = qualityFromResiduals(result.model.residuals, threshold,
        result.model.source_indices, result.model.inlier_indices);
    result.valid = result.model.quality.valid;
    return result;
}
