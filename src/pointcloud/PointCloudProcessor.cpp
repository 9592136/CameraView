#include "PointCloudProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

namespace {

struct CellKey {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t z = 0;
    bool operator==(const CellKey& other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct CellKeyHash {
    std::size_t operator()(const CellKey& key) const
    {
        std::size_t seed = std::hash<std::int64_t>{}(key.x);
        seed ^= std::hash<std::int64_t>{}(key.y) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<std::int64_t>{}(key.z) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};

CellKey CellFor(const PointCloudPoint& point, double size)
{
    return {
        static_cast<std::int64_t>(std::floor(point.x / size)),
        static_cast<std::int64_t>(std::floor(point.y / size)),
        static_cast<std::int64_t>(std::floor(point.z / size))};
}

struct Accumulator {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    std::size_t count = 0;
    std::size_t color_count = 0;
};

PointCloud DerivedCloud(const PointCloud& source)
{
    PointCloud result;
    result.unit = source.unit;
    result.name = source.name;
    result.source_path = source.source_path;
    return result;
}

std::array<double, 3> SmallestEigenvector(
    std::array<std::array<double, 3>, 3> matrix,
    std::array<double, 3>& vector)
{
    std::array<std::array<double, 3>, 3> eigenvectors{{
        {{1.0, 0.0, 0.0}},
        {{0.0, 1.0, 0.0}},
        {{0.0, 0.0, 1.0}}}};
    for (int iteration = 0; iteration < 32; ++iteration) {
        int p = 0;
        int q = 1;
        double largest = std::abs(matrix[0][1]);
        for (int row = 0; row < 3; ++row) {
            for (int column = row + 1; column < 3; ++column) {
                const double value = std::abs(matrix[row][column]);
                if (value > largest) {
                    largest = value;
                    p = row;
                    q = column;
                }
            }
        }
        if (largest < 1e-14) break;
        const double angle = 0.5 * std::atan2(
            2.0 * matrix[p][q], matrix[q][q] - matrix[p][p]);
        const double cosine = std::cos(angle);
        const double sine = std::sin(angle);
        const double app = cosine * cosine * matrix[p][p] -
            2.0 * sine * cosine * matrix[p][q] + sine * sine * matrix[q][q];
        const double aqq = sine * sine * matrix[p][p] +
            2.0 * sine * cosine * matrix[p][q] + cosine * cosine * matrix[q][q];
        for (int index = 0; index < 3; ++index) {
            if (index == p || index == q) continue;
            const double aip = cosine * matrix[index][p] - sine * matrix[index][q];
            const double aiq = sine * matrix[index][p] + cosine * matrix[index][q];
            matrix[index][p] = matrix[p][index] = aip;
            matrix[index][q] = matrix[q][index] = aiq;
        }
        matrix[p][p] = app;
        matrix[q][q] = aqq;
        matrix[p][q] = matrix[q][p] = 0.0;
        for (int row = 0; row < 3; ++row) {
            const double vip = cosine * eigenvectors[row][p] - sine * eigenvectors[row][q];
            const double viq = sine * eigenvectors[row][p] + cosine * eigenvectors[row][q];
            eigenvectors[row][p] = vip;
            eigenvectors[row][q] = viq;
        }
    }
    int smallest = 0;
    if (matrix[1][1] < matrix[smallest][smallest]) smallest = 1;
    if (matrix[2][2] < matrix[smallest][smallest]) smallest = 2;
    vector = {eigenvectors[0][smallest], eigenvectors[1][smallest],
        eigenvectors[2][smallest]};
    return {matrix[0][0], matrix[1][1], matrix[2][2]};
}

} // namespace

double PointCloudPlane::SignedDistance(const PointCloudPoint& point) const
{
    return valid ? nx * point.x + ny * point.y + nz * point.z + d : 0.0;
}

PointCloud PointCloudProcessor::VoxelDownsample(const PointCloud& cloud, double voxel_size)
{
    if (cloud.Empty() || voxel_size <= 0.0) return cloud;
    std::unordered_map<CellKey, Accumulator, CellKeyHash> cells;
    cells.reserve(cloud.points.size());
    for (const PointCloudPoint& point : cloud.points) {
        Accumulator& cell = cells[CellFor(point, voxel_size)];
        cell.x += point.x;
        cell.y += point.y;
        cell.z += point.z;
        ++cell.count;
        if (point.has_color) {
            cell.r += point.r;
            cell.g += point.g;
            cell.b += point.b;
            ++cell.color_count;
        }
    }
    PointCloud result = DerivedCloud(cloud);
    result.points.reserve(cells.size());
    for (const auto& entry : cells) {
        const Accumulator& cell = entry.second;
        PointCloudPoint point;
        point.x = cell.x / cell.count;
        point.y = cell.y / cell.count;
        point.z = cell.z / cell.count;
        if (cell.color_count > 0) {
            point.r = static_cast<std::uint8_t>(std::lround(cell.r / cell.color_count));
            point.g = static_cast<std::uint8_t>(std::lround(cell.g / cell.color_count));
            point.b = static_cast<std::uint8_t>(std::lround(cell.b / cell.color_count));
            point.has_color = true;
        }
        result.points.push_back(point);
    }
    result.RecalculateBounds();
    return result;
}

PointCloud PointCloudProcessor::Crop(const PointCloud& cloud, const PointCloudBounds& bounds)
{
    PointCloud result = DerivedCloud(cloud);
    if (!bounds.valid) return result;
    result.points.reserve(cloud.points.size());
    std::copy_if(cloud.points.begin(), cloud.points.end(), std::back_inserter(result.points),
        [&bounds](const PointCloudPoint& point) {
            return point.x >= bounds.min_x && point.x <= bounds.max_x &&
                point.y >= bounds.min_y && point.y <= bounds.max_y &&
                point.z >= bounds.min_z && point.z <= bounds.max_z;
        });
    result.RecalculateBounds();
    return result;
}

PointCloud PointCloudProcessor::SelectIndices(
    const PointCloud& cloud,
    const std::vector<std::size_t>& indices,
    bool keep_selected)
{
    PointCloud result = DerivedCloud(cloud);
    std::vector<bool> selected(cloud.points.size(), false);
    for (std::size_t index : indices) {
        if (index < selected.size()) selected[index] = true;
    }
    result.points.reserve(keep_selected ? indices.size() : cloud.points.size());
    for (std::size_t index = 0; index < cloud.points.size(); ++index) {
        if (selected[index] == keep_selected) result.points.push_back(cloud.points[index]);
    }
    result.RecalculateBounds();
    return result;
}

PointCloud PointCloudProcessor::RemoveRadiusOutliers(
    const PointCloud& cloud,
    double radius,
    std::size_t minimum_neighbors)
{
    if (cloud.Empty() || radius <= 0.0 || minimum_neighbors == 0) return cloud;
    std::unordered_map<CellKey, std::vector<std::size_t>, CellKeyHash> grid;
    grid.reserve(cloud.points.size());
    for (std::size_t index = 0; index < cloud.points.size(); ++index) {
        grid[CellFor(cloud.points[index], radius)].push_back(index);
    }
    const double radius_squared = radius * radius;
    PointCloud result = DerivedCloud(cloud);
    result.points.reserve(cloud.points.size());
    for (std::size_t index = 0; index < cloud.points.size(); ++index) {
        const PointCloudPoint& point = cloud.points[index];
        const CellKey center = CellFor(point, radius);
        std::size_t neighbors = 0;
        for (int dz = -1; dz <= 1 && neighbors < minimum_neighbors; ++dz) {
            for (int dy = -1; dy <= 1 && neighbors < minimum_neighbors; ++dy) {
                for (int dx = -1; dx <= 1 && neighbors < minimum_neighbors; ++dx) {
                    const auto cell = grid.find({center.x + dx, center.y + dy, center.z + dz});
                    if (cell == grid.end()) continue;
                    for (std::size_t candidate : cell->second) {
                        if (candidate == index) continue;
                        const PointCloudPoint& other = cloud.points[candidate];
                        const double delta_x = point.x - other.x;
                        const double delta_y = point.y - other.y;
                        const double delta_z = point.z - other.z;
                        if (delta_x * delta_x + delta_y * delta_y + delta_z * delta_z <=
                            radius_squared) {
                            ++neighbors;
                            if (neighbors >= minimum_neighbors) break;
                        }
                    }
                }
            }
        }
        if (neighbors >= minimum_neighbors) result.points.push_back(point);
    }
    result.RecalculateBounds();
    return result;
}

PointCloudPlane PointCloudProcessor::FitPlane(const PointCloud& cloud)
{
    PointCloudPlane plane;
    if (cloud.points.size() < 3) return plane;
    const PointCloudCentroid center = cloud.Centroid();
    std::array<std::array<double, 3>, 3> covariance{};
    for (const PointCloudPoint& point : cloud.points) {
        const std::array<double, 3> delta{
            point.x - center.x, point.y - center.y, point.z - center.z};
        for (int row = 0; row < 3; ++row) {
            for (int column = row; column < 3; ++column) {
                covariance[row][column] += delta[row] * delta[column];
            }
        }
    }
    covariance[1][0] = covariance[0][1];
    covariance[2][0] = covariance[0][2];
    covariance[2][1] = covariance[1][2];
    std::array<double, 3> normal{};
    const std::array<double, 3> eigenvalues = SmallestEigenvector(covariance, normal);
    std::array<double, 3> sorted_eigenvalues = eigenvalues;
    std::sort(sorted_eigenvalues.begin(), sorted_eigenvalues.end());
    const double covariance_scale = std::max(sorted_eigenvalues[2], 1.0);
    if (sorted_eigenvalues[1] <= covariance_scale * 1e-12) return plane;
    const double length = std::sqrt(
        normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
    if (length <= std::numeric_limits<double>::epsilon()) return plane;
    plane.nx = normal[0] / length;
    plane.ny = normal[1] / length;
    plane.nz = normal[2] / length;
    if (plane.nz < 0.0) {
        plane.nx = -plane.nx;
        plane.ny = -plane.ny;
        plane.nz = -plane.nz;
    }
    plane.d = -(plane.nx * center.x + plane.ny * center.y + plane.nz * center.z);
    plane.valid = true;
    double squared_error = 0.0;
    for (const PointCloudPoint& point : cloud.points) {
        const double distance = plane.SignedDistance(point);
        squared_error += distance * distance;
    }
    plane.rms = std::sqrt(squared_error / cloud.points.size());
    return plane;
}

PointCloud PointCloudProcessor::LevelToPlane(
    const PointCloud& cloud,
    const PointCloudPlane& plane)
{
    if (cloud.Empty() || !plane.valid) return cloud;
    PointCloud result = DerivedCloud(cloud);
    result.points.reserve(cloud.points.size());
    const PointCloudCentroid center = cloud.Centroid();
    const double axis_x = plane.ny;
    const double axis_y = -plane.nx;
    const double sine = std::sqrt(axis_x * axis_x + axis_y * axis_y);
    const double cosine = std::clamp(plane.nz, -1.0, 1.0);
    const double unit_x = sine > 1e-12 ? axis_x / sine : 1.0;
    const double unit_y = sine > 1e-12 ? axis_y / sine : 0.0;
    for (const PointCloudPoint& source : cloud.points) {
        const double x = source.x - center.x;
        const double y = source.y - center.y;
        const double z = source.z - center.z;
        const double dot = unit_x * x + unit_y * y;
        PointCloudPoint point = source;
        point.x = center.x + x * cosine + unit_y * z * sine + unit_x * dot * (1.0 - cosine);
        point.y = center.y + y * cosine - unit_x * z * sine + unit_y * dot * (1.0 - cosine);
        point.z = center.z - unit_y * x * sine + unit_x * y * sine + z * cosine;
        result.points.push_back(point);
    }
    result.RecalculateBounds();
    return result;
}
