#include "PointCloudProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
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

struct Cell2Key {
    std::int64_t x = 0;
    std::int64_t y = 0;
    bool operator==(const Cell2Key& other) const { return x == other.x && y == other.y; }
};

struct Cell2KeyHash {
    std::size_t operator()(const Cell2Key& key) const
    {
        std::size_t seed = std::hash<std::int64_t>{}(key.x);
        seed ^= std::hash<std::int64_t>{}(key.y) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};

Cell2Key CellForXY(const PointCloudPoint& point, double size)
{
    return {static_cast<std::int64_t>(std::floor(point.x / size)),
        static_cast<std::int64_t>(std::floor(point.y / size))};
}

double Median(std::vector<double>& values)
{
    if (values.empty()) return 0.0;
    const std::size_t middle = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + middle, values.end());
    const double upper = values[middle];
    if (values.size() % 2 != 0) return upper;
    std::nth_element(values.begin(), values.begin() + middle - 1, values.begin() + middle);
    return (values[middle - 1] + upper) * 0.5;
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

double PointCloudProcessor::EstimateNominalSpacing(const PointCloud& cloud)
{
    if (cloud.points.size() < 2) return 0.0;
    const double extent = std::max(cloud.bounds.Width(), cloud.bounds.Depth());
    if (extent <= 0.0) return 0.0;
    const double coarse_cell = extent /
        std::max(8.0, std::sqrt(static_cast<double>(cloud.points.size())) * 0.5);
    if (coarse_cell <= 0.0) return 0.0;
    std::unordered_map<Cell2Key, std::vector<std::size_t>, Cell2KeyHash> grid;
    grid.reserve(cloud.points.size());
    for (std::size_t index = 0; index < cloud.points.size(); ++index) {
        grid[CellForXY(cloud.points[index], coarse_cell)].push_back(index);
    }
    const std::size_t sample_stride = std::max<std::size_t>(1, cloud.points.size() / 4096);
    std::vector<double> distances;
    distances.reserve(std::min<std::size_t>(4096, cloud.points.size()));
    for (std::size_t index = 0; index < cloud.points.size(); index += sample_stride) {
        const PointCloudPoint& point = cloud.points[index];
        const Cell2Key center = CellForXY(point, coarse_cell);
        double nearest = std::numeric_limits<double>::infinity();
        for (int dy = -2; dy <= 2; ++dy) {
            for (int dx = -2; dx <= 2; ++dx) {
                const auto cell = grid.find({center.x + dx, center.y + dy});
                if (cell == grid.end()) continue;
                for (std::size_t candidate : cell->second) {
                    if (candidate == index) continue;
                    const double delta_x = point.x - cloud.points[candidate].x;
                    const double delta_y = point.y - cloud.points[candidate].y;
                    const double distance = std::sqrt(delta_x * delta_x + delta_y * delta_y);
                    if (distance > 1e-15) nearest = std::min(nearest, distance);
                }
            }
        }
        if (std::isfinite(nearest)) distances.push_back(nearest);
    }
    return Median(distances);
}

PointCloud PointCloudProcessor::SmartDenoise(
    const PointCloud& cloud,
    const PointCloudDenoiseOptions& options,
    PointCloudDenoiseReport* report)
{
    PointCloudDenoiseReport local_report;
    if (cloud.Empty()) {
        if (report) *report = local_report;
        return cloud;
    }
    const double spacing = EstimateNominalSpacing(cloud);
    const double radius = options.neighbor_radius > 0.0
        ? options.neighbor_radius
        : spacing * 2.5;
    if (radius <= 0.0) {
        if (report) *report = local_report;
        return cloud;
    }
    std::unordered_map<Cell2Key, std::vector<std::size_t>, Cell2KeyHash> grid;
    grid.reserve(cloud.points.size());
    for (std::size_t index = 0; index < cloud.points.size(); ++index) {
        grid[CellForXY(cloud.points[index], radius)].push_back(index);
    }
    std::vector<bool> keep(cloud.points.size(), true);
    std::vector<double> robust_z(cloud.points.size(), 0.0);
    std::vector<double> smoothing_limits(cloud.points.size(), 0.0);
    const double radius_squared = radius * radius;
    for (std::size_t index = 0; index < cloud.points.size(); ++index) {
        const PointCloudPoint& point = cloud.points[index];
        const Cell2Key center = CellForXY(point, radius);
        std::vector<double> heights;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const auto cell = grid.find({center.x + dx, center.y + dy});
                if (cell == grid.end()) continue;
                for (std::size_t candidate : cell->second) {
                    if (candidate == index) continue;
                    const PointCloudPoint& other = cloud.points[candidate];
                    const double delta_x = point.x - other.x;
                    const double delta_y = point.y - other.y;
                    if (delta_x * delta_x + delta_y * delta_y <= radius_squared) {
                        heights.push_back(other.z);
                    }
                }
            }
        }
        if (heights.size() < options.minimum_neighbors) {
            keep[index] = false;
            ++local_report.removed_isolated;
            continue;
        }
        const double median = Median(heights);
        robust_z[index] = median;
        std::vector<double> deviations;
        deviations.reserve(heights.size());
        for (double height : heights) deviations.push_back(std::abs(height - median));
        const double mad = Median(deviations);
        const double smoothing_limit = options.minimum_height_deviation > 0.0
            ? options.minimum_height_deviation
            : std::max(spacing * 0.2, 1e-12);
        smoothing_limits[index] = smoothing_limit;
        const double threshold = std::max(smoothing_limit,
            options.spike_sigma * 1.4826 * mad);
        const std::size_t same_surface_support = static_cast<std::size_t>(std::count_if(
            heights.begin(), heights.end(), [point, threshold](double height) {
                return std::abs(height - point.z) <= threshold;
            }));
        const std::size_t required_surface_support =
            std::max<std::size_t>(2, options.minimum_neighbors / 2);
        if (std::abs(point.z - median) > threshold &&
            same_surface_support < required_surface_support) {
            keep[index] = false;
            ++local_report.removed_spikes;
        }
    }
    PointCloud result = DerivedCloud(cloud);
    result.points.reserve(cloud.points.size() -
        local_report.removed_isolated - local_report.removed_spikes);
    for (std::size_t index = 0; index < cloud.points.size(); ++index) {
        if (!keep[index]) continue;
        PointCloudPoint point = cloud.points[index];
        if (options.smoothing_strength > 0.0 &&
            std::abs(point.z - robust_z[index]) <= smoothing_limits[index]) {
            point.z += std::clamp(options.smoothing_strength, 0.0, 1.0) *
                (robust_z[index] - point.z);
            ++local_report.smoothed_points;
        }
        result.points.push_back(point);
    }
    // Additional iterations are intentionally conservative and reuse the same
    // robust pass, preserving sharp edges beyond the configured deviation.
    for (int iteration = 1; iteration < options.smoothing_iterations; ++iteration) {
        PointCloudDenoiseOptions next = options;
        next.smoothing_iterations = 1;
        next.minimum_neighbors = 1;
        PointCloudDenoiseReport ignored;
        result = SmartDenoise(result, next, &ignored);
        local_report.smoothed_points += ignored.smoothed_points;
    }
    result.RecalculateBounds();
    if (report) *report = local_report;
    return result;
}

PointCloud PointCloudProcessor::RepairHoles(
    const PointCloud& cloud,
    const PointCloudHoleRepairOptions& options,
    PointCloudHoleRepairReport* report)
{
    PointCloudHoleRepairReport local_report;
    if (cloud.Empty()) {
        if (report) *report = local_report;
        return cloud;
    }
    const double spacing = options.grid_spacing > 0.0
        ? options.grid_spacing
        : EstimateNominalSpacing(cloud);
    if (spacing <= 0.0 || cloud.bounds.Width() <= 0.0 || cloud.bounds.Depth() <= 0.0) {
        local_report.applicable = false;
        local_report.message = L"The point cloud cannot be represented as a 2.5D XY surface.";
        if (report) *report = local_report;
        return cloud;
    }
    const std::size_t columns = static_cast<std::size_t>(
        std::floor(cloud.bounds.Width() / spacing + 0.5)) + 1;
    const std::size_t rows = static_cast<std::size_t>(
        std::floor(cloud.bounds.Depth() / spacing + 0.5)) + 1;
    if (columns < 3 || rows < 3 || columns > options.maximum_grid_cells / rows) {
        local_report.applicable = false;
        local_report.message = L"The repair grid would be too large; increase grid spacing.";
        if (report) *report = local_report;
        return cloud;
    }
    struct GridCell {
        double z_sum = 0.0;
        double r_sum = 0.0;
        double g_sum = 0.0;
        double b_sum = 0.0;
        std::size_t count = 0;
        std::size_t color_count = 0;
    };
    const std::size_t cell_count = rows * columns;
    std::vector<GridCell> cells(cell_count);
    for (const PointCloudPoint& point : cloud.points) {
        const std::size_t x = std::min(columns - 1, static_cast<std::size_t>(
            std::llround((point.x - cloud.bounds.min_x) / spacing)));
        const std::size_t y = std::min(rows - 1, static_cast<std::size_t>(
            std::llround((point.y - cloud.bounds.min_y) / spacing)));
        GridCell& cell = cells[y * columns + x];
        cell.z_sum += point.z;
        ++cell.count;
        if (point.has_color) {
            cell.r_sum += point.r;
            cell.g_sum += point.g;
            cell.b_sum += point.b;
            ++cell.color_count;
        }
    }
    std::vector<bool> exterior(cell_count, false);
    std::queue<std::size_t> queue;
    auto enqueueExterior = [&](std::size_t index) {
        if (cells[index].count == 0 && !exterior[index]) {
            exterior[index] = true;
            queue.push(index);
        }
    };
    for (std::size_t x = 0; x < columns; ++x) {
        enqueueExterior(x);
        enqueueExterior((rows - 1) * columns + x);
    }
    for (std::size_t y = 0; y < rows; ++y) {
        enqueueExterior(y * columns);
        enqueueExterior(y * columns + columns - 1);
    }
    constexpr int neighbors4[4][2]{{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    while (!queue.empty()) {
        const std::size_t index = queue.front();
        queue.pop();
        const std::size_t x = index % columns;
        const std::size_t y = index / columns;
        for (const auto& offset : neighbors4) {
            const std::int64_t nx = static_cast<std::int64_t>(x) + offset[0];
            const std::int64_t ny = static_cast<std::int64_t>(y) + offset[1];
            if (nx < 0 || ny < 0 || nx >= static_cast<std::int64_t>(columns) ||
                ny >= static_cast<std::int64_t>(rows)) continue;
            const std::size_t next = static_cast<std::size_t>(ny) * columns +
                static_cast<std::size_t>(nx);
            if (cells[next].count == 0 && !exterior[next]) {
                exterior[next] = true;
                queue.push(next);
            }
        }
    }
    std::vector<bool> visited(cell_count, false);
    std::vector<std::size_t> fill_cells;
    for (std::size_t start = 0; start < cell_count; ++start) {
        if (cells[start].count != 0 || exterior[start] || visited[start]) continue;
        std::vector<std::size_t> component;
        visited[start] = true;
        queue.push(start);
        while (!queue.empty()) {
            const std::size_t index = queue.front();
            queue.pop();
            component.push_back(index);
            const std::size_t x = index % columns;
            const std::size_t y = index / columns;
            for (const auto& offset : neighbors4) {
                const std::int64_t nx = static_cast<std::int64_t>(x) + offset[0];
                const std::int64_t ny = static_cast<std::int64_t>(y) + offset[1];
                if (nx < 0 || ny < 0 || nx >= static_cast<std::int64_t>(columns) ||
                    ny >= static_cast<std::int64_t>(rows)) continue;
                const std::size_t next = static_cast<std::size_t>(ny) * columns +
                    static_cast<std::size_t>(nx);
                if (cells[next].count == 0 && !exterior[next] && !visited[next]) {
                    visited[next] = true;
                    queue.push(next);
                }
            }
        }
        ++local_report.detected_holes;
        if (component.size() <= options.maximum_hole_cells) {
            fill_cells.insert(fill_cells.end(), component.begin(), component.end());
            ++local_report.filled_holes;
        } else {
            ++local_report.skipped_large_holes;
        }
    }
    PointCloud result = cloud;
    result.points.reserve(cloud.points.size() + fill_cells.size());
    const std::size_t no_point = std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> generated_indices(cell_count, no_point);
    for (std::size_t index : fill_cells) {
        const std::size_t grid_x = index % columns;
        const std::size_t grid_y = index / columns;
        const double x = cloud.bounds.min_x + grid_x * spacing;
        const double y = cloud.bounds.min_y + grid_y * spacing;
        double matrix[3][4]{};
        double color_r = 0.0;
        double color_g = 0.0;
        double color_b = 0.0;
        double color_weight = 0.0;
        int support = 0;
        bool left = false, right = false, above = false, below = false;
        for (int dy = -options.search_radius_cells; dy <= options.search_radius_cells; ++dy) {
            for (int dx = -options.search_radius_cells; dx <= options.search_radius_cells; ++dx) {
                if (dx == 0 && dy == 0) continue;
                const std::int64_t nx = static_cast<std::int64_t>(grid_x) + dx;
                const std::int64_t ny = static_cast<std::int64_t>(grid_y) + dy;
                if (nx < 0 || ny < 0 || nx >= static_cast<std::int64_t>(columns) ||
                    ny >= static_cast<std::int64_t>(rows)) continue;
                const GridCell& cell = cells[static_cast<std::size_t>(ny) * columns +
                    static_cast<std::size_t>(nx)];
                if (cell.count == 0) continue;
                const double sample_x = cloud.bounds.min_x + nx * spacing;
                const double sample_y = cloud.bounds.min_y + ny * spacing;
                const double sample_z = cell.z_sum / cell.count;
                const double distance_squared = (sample_x - x) * (sample_x - x) +
                    (sample_y - y) * (sample_y - y);
                const double weight = 1.0 / std::max(distance_squared, spacing * spacing * 0.25);
                const double values[3]{sample_x, sample_y, 1.0};
                for (int row = 0; row < 3; ++row) {
                    for (int column = 0; column < 3; ++column) {
                        matrix[row][column] += weight * values[row] * values[column];
                    }
                    matrix[row][3] += weight * values[row] * sample_z;
                }
                if (cell.color_count > 0) {
                    color_r += weight * cell.r_sum / cell.color_count;
                    color_g += weight * cell.g_sum / cell.color_count;
                    color_b += weight * cell.b_sum / cell.color_count;
                    color_weight += weight;
                }
                left = left || dx < 0;
                right = right || dx > 0;
                above = above || dy < 0;
                below = below || dy > 0;
                ++support;
            }
        }
        std::array<double, 3> coefficients{};
        if (support < 6 || !left || !right || !above || !below ||
            !Solve3x3(matrix, coefficients)) continue;
        PointCloudPoint point;
        point.x = x;
        point.y = y;
        point.z = coefficients[0] * x + coefficients[1] * y + coefficients[2];
        if (color_weight > 0.0) {
            point.r = static_cast<std::uint8_t>(std::clamp(std::lround(color_r / color_weight), 0L, 255L));
            point.g = static_cast<std::uint8_t>(std::clamp(std::lround(color_g / color_weight), 0L, 255L));
            point.b = static_cast<std::uint8_t>(std::clamp(std::lround(color_b / color_weight), 0L, 255L));
            point.has_color = true;
        }
        generated_indices[index] = result.points.size();
        result.points.push_back(point);
        ++local_report.filled_points;
    }
    for (int iteration = 0; iteration < options.smoothing_iterations; ++iteration) {
        std::vector<std::pair<std::size_t, double>> updates;
        updates.reserve(local_report.filled_points);
        for (std::size_t cell_index : fill_cells) {
            const std::size_t point_index = generated_indices[cell_index];
            if (point_index == no_point) continue;
            const std::size_t grid_x = cell_index % columns;
            const std::size_t grid_y = cell_index / columns;
            double sum = 0.0;
            int count = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    const std::int64_t nx = static_cast<std::int64_t>(grid_x) + dx;
                    const std::int64_t ny = static_cast<std::int64_t>(grid_y) + dy;
                    if (nx < 0 || ny < 0 || nx >= static_cast<std::int64_t>(columns) ||
                        ny >= static_cast<std::int64_t>(rows)) continue;
                    const std::size_t neighbor_cell = static_cast<std::size_t>(ny) * columns +
                        static_cast<std::size_t>(nx);
                    if (cells[neighbor_cell].count > 0) {
                        sum += cells[neighbor_cell].z_sum / cells[neighbor_cell].count;
                        ++count;
                    } else if (generated_indices[neighbor_cell] != no_point) {
                        sum += result.points[generated_indices[neighbor_cell]].z;
                        ++count;
                    }
                }
            }
            if (count >= 3) updates.emplace_back(point_index, sum / count);
        }
        for (const auto& update : updates) {
            result.points[update.first].z =
                result.points[update.first].z * 0.5 + update.second * 0.5;
        }
    }
    result.RecalculateBounds();
    if (report) *report = local_report;
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
