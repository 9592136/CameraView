#include "pointcloud/PointCloud.h"
#include "pointcloud/PointCloudIO.h"
#include "pointcloud/PointCloudMeasurement.h"
#include "pointcloud/PointCloudProcessor.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

int fail(const char* message)
{
    std::cerr << message << '\n';
    return 1;
}

bool near(double left, double right, double tolerance = 1e-6)
{
    return std::abs(left - right) <= tolerance;
}

PointCloud makePlaneCloud()
{
    PointCloud cloud;
    cloud.name = L"synthetic plane";
    cloud.unit = PointCloudUnit::Millimeters;
    for (int y = -10; y <= 10; ++y) {
        for (int x = -10; x <= 10; ++x) {
            PointCloudPoint point;
            point.x = x * 0.5;
            point.y = y * 0.5;
            point.z = 2.0 + 0.25 * point.x - 0.1 * point.y;
            point.r = static_cast<std::uint8_t>(100 + x + 10);
            point.g = static_cast<std::uint8_t>(120 + y + 10);
            point.b = 200;
            point.has_color = true;
            cloud.points.push_back(point);
        }
    }
    cloud.RecalculateBounds();
    return cloud;
}

} // namespace

int main()
{
    PointCloud cloud = makePlaneCloud();
    if (cloud.Size() != 441 || !cloud.bounds.valid ||
        !near(cloud.bounds.Width(), 10.0) || !near(cloud.bounds.Depth(), 10.0) ||
        cloud.unit != PointCloudUnit::Millimeters ||
        std::wstring(PointCloudUnitLabel(cloud.unit)) != L"mm" ||
        !near(PointCloudUnitToMeters(cloud.unit), 0.001)) {
        return fail("Point-cloud bounds, unit, or metadata are incorrect.");
    }

    const PointCloudPlane plane = PointCloudProcessor::FitPlane(cloud);
    const double normal_length = std::sqrt(1.0 + 0.25 * 0.25 + 0.1 * 0.1);
    if (!plane.valid || plane.rms > 1e-8 ||
        !near(plane.nx, -0.25 / normal_length, 1e-6) ||
        !near(plane.ny, 0.1 / normal_length, 1e-6) ||
        !near(plane.nz, 1.0 / normal_length, 1e-6)) {
        return fail("Least-squares plane fitting is inaccurate.");
    }
    PointCloud collinear;
    collinear.points = {{0.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, {2.0, 2.0, 2.0}};
    collinear.RecalculateBounds();
    if (PointCloudProcessor::FitPlane(collinear).valid) {
        return fail("Plane fitting accepted a degenerate collinear point set.");
    }
    const PointCloud leveled = PointCloudProcessor::LevelToPlane(cloud, plane);
    if (leveled.Empty() || leveled.bounds.Height() > 1e-7) {
        return fail("Plane leveling did not flatten the fitted surface.");
    }

    const PointCloud downsampled = PointCloudProcessor::VoxelDownsample(cloud, 1.1);
    if (downsampled.Empty() || downsampled.Size() >= cloud.Size() ||
        downsampled.unit != cloud.unit || !downsampled.points.front().has_color) {
        return fail("Voxel downsampling did not preserve point-cloud metadata and colors.");
    }
    PointCloudBounds crop_bounds;
    crop_bounds.min_x = -1.0;
    crop_bounds.max_x = 1.0;
    crop_bounds.min_y = -1.0;
    crop_bounds.max_y = 1.0;
    crop_bounds.min_z = cloud.bounds.min_z;
    crop_bounds.max_z = cloud.bounds.max_z;
    crop_bounds.valid = true;
    const PointCloud cropped = PointCloudProcessor::Crop(cloud, crop_bounds);
    if (cropped.Size() != 25 || cropped.bounds.min_x < -1.0 || cropped.bounds.max_x > 1.0) {
        return fail("Coordinate crop did not retain the expected points.");
    }
    const std::vector<std::size_t> selected_indices{0, 10, 20};
    const PointCloud selected_only = PointCloudProcessor::SelectIndices(
        cloud, selected_indices, true);
    const PointCloud selected_removed = PointCloudProcessor::SelectIndices(
        cloud, selected_indices, false);
    if (selected_only.Size() != 3 || selected_removed.Size() != cloud.Size() - 3 ||
        !near(selected_only.points[1].x, cloud.points[10].x)) {
        return fail("Interactive selection crop did not preserve the expected points.");
    }

    PointCloud noisy = cloud;
    noisy.points.push_back({100.0, 100.0, 100.0});
    noisy.RecalculateBounds();
    const PointCloud filtered = PointCloudProcessor::RemoveRadiusOutliers(noisy, 0.8, 3);
    if (filtered.Size() != cloud.Size() || filtered.bounds.max_x > 10.0) {
        return fail("Radius outlier filtering did not remove the isolated point.");
    }

    PointCloud denoise_cloud;
    denoise_cloud.unit = PointCloudUnit::Millimeters;
    for (int y = 0; y < 9; ++y) {
        for (int x = 0; x < 9; ++x) {
            PointCloudPoint point;
            point.x = x;
            point.y = y;
            point.z = 0.05 * x + 0.02 * y;
            if (x == 4 && y == 4) point.z += 8.0;
            denoise_cloud.points.push_back(point);
        }
    }
    denoise_cloud.points.push_back({100.0, 100.0, 20.0});
    denoise_cloud.RecalculateBounds();
    PointCloudDenoiseOptions denoise_options;
    denoise_options.neighbor_radius = 1.6;
    denoise_options.minimum_neighbors = 3;
    denoise_options.minimum_height_deviation = 0.3;
    denoise_options.spike_sigma = 3.0;
    denoise_options.smoothing_strength = 0.0;
    PointCloudDenoiseReport denoise_report;
    const PointCloud smart_filtered = PointCloudProcessor::SmartDenoise(
        denoise_cloud, denoise_options, &denoise_report);
    if (smart_filtered.Size() != 80 || denoise_report.removed_isolated != 1 ||
        denoise_report.removed_spikes != 1 || smart_filtered.bounds.max_x > 8.0) {
        return fail("Smart denoising did not remove both the isolated point and local spike.");
    }
    PointCloud step_edge;
    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < 10; ++x) {
            step_edge.points.push_back({static_cast<double>(x), static_cast<double>(y),
                x < 5 ? 0.0 : 5.0});
        }
    }
    step_edge.RecalculateBounds();
    PointCloudDenoiseReport edge_denoise_report;
    const PointCloud preserved_edge = PointCloudProcessor::SmartDenoise(
        step_edge, denoise_options, &edge_denoise_report);
    if (preserved_edge.Size() != step_edge.Size() || edge_denoise_report.removed_spikes != 0 ||
        !near(preserved_edge.bounds.Height(), 5.0)) {
        return fail("Smart denoising incorrectly removed a supported sharp surface edge.");
    }

    PointCloud hole_cloud;
    hole_cloud.unit = PointCloudUnit::Millimeters;
    for (int y = 0; y <= 10; ++y) {
        for (int x = 0; x <= 10; ++x) {
            if (x >= 4 && x <= 6 && y >= 4 && y <= 6) continue;
            PointCloudPoint point;
            point.x = x;
            point.y = y;
            point.z = 2.0 + 0.2 * x - 0.1 * y;
            hole_cloud.points.push_back(point);
        }
    }
    hole_cloud.RecalculateBounds();
    PointCloudHoleRepairOptions repair_options;
    repair_options.grid_spacing = 1.0;
    repair_options.maximum_hole_cells = 16;
    repair_options.search_radius_cells = 4;
    repair_options.smoothing_iterations = 2;
    PointCloudHoleRepairReport repair_report;
    const PointCloud repaired = PointCloudProcessor::RepairHoles(
        hole_cloud, repair_options, &repair_report);
    if (repaired.Size() != 121 || repair_report.detected_holes != 1 ||
        repair_report.filled_holes != 1 || repair_report.filled_points != 9) {
        return fail("Internal point-cloud hole detection or repair produced the wrong topology.");
    }
    const auto repaired_center = std::find_if(repaired.points.begin(), repaired.points.end(),
        [](const PointCloudPoint& point) { return near(point.x, 5.0) && near(point.y, 5.0); });
    if (repaired_center == repaired.points.end() || !near(repaired_center->z, 2.5, 1e-5)) {
        return fail("Hole repair did not follow the surrounding planar trend.");
    }
    PointCloud edge_gap = repaired;
    edge_gap.points.erase(std::remove_if(edge_gap.points.begin(), edge_gap.points.end(),
        [](const PointCloudPoint& point) { return near(point.x, 0.0) && near(point.y, 5.0); }),
        edge_gap.points.end());
    edge_gap.RecalculateBounds();
    PointCloudHoleRepairReport edge_report;
    const PointCloud edge_unchanged = PointCloudProcessor::RepairHoles(
        edge_gap, repair_options, &edge_report);
    if (edge_unchanged.Size() != edge_gap.Size() || edge_report.filled_points != 0) {
        return fail("Hole repair incorrectly expanded an exterior boundary gap.");
    }
    PointCloudHoleRepairOptions skip_large_options = repair_options;
    skip_large_options.maximum_hole_cells = 4;
    PointCloudHoleRepairReport skipped_report;
    const PointCloud skipped_hole = PointCloudProcessor::RepairHoles(
        hole_cloud, skip_large_options, &skipped_report);
    if (skipped_hole.Size() != hole_cloud.Size() || skipped_report.filled_points != 0 ||
        skipped_report.skipped_large_holes != 1) {
        return fail("Hole repair did not honor the maximum automatic repair area.");
    }

    PointCloud performance_cloud;
    performance_cloud.points.reserve(256 * 256 + 1);
    for (int y = 0; y < 256; ++y) {
        for (int x = 0; x < 256; ++x) {
            performance_cloud.points.push_back({
                static_cast<double>(x), static_cast<double>(y),
                0.002 * x + 0.001 * y});
        }
    }
    performance_cloud.points.push_back({1000.0, 1000.0, 100.0});
    performance_cloud.RecalculateBounds();
    PointCloudDenoiseOptions performance_options;
    performance_options.neighbor_radius = 2.1;
    performance_options.minimum_neighbors = 4;
    performance_options.minimum_height_deviation = 0.05;
    performance_options.smoothing_strength = 0.0;
    const auto performance_start = std::chrono::steady_clock::now();
    PointCloudDenoiseReport performance_report;
    const PointCloud performance_result = PointCloudProcessor::SmartDenoise(
        performance_cloud, performance_options, &performance_report);
    const auto performance_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - performance_start);
    if (performance_result.Size() != 256U * 256U ||
        performance_report.removed_isolated != 1 || performance_elapsed.count() > 5000) {
        return fail("Smart denoising did not meet the large-cloud fast-path contract.");
    }

    const PointCloudPoint origin{0.0, 0.0, 0.0};
    const PointCloudPoint x_axis{3.0, 0.0, 0.0};
    const PointCloudPoint y_axis{0.0, 4.0, 0.0};
    const PointCloudPoint elevated{0.0, 0.0, 5.0};
    if (!near(PointCloudMeasurement::Distance(x_axis, y_axis), 5.0) ||
        !near(PointCloudMeasurement::HeightDifference(origin, elevated), 5.0) ||
        !near(PointCloudMeasurement::AngleDegrees(x_axis, origin, y_axis), 90.0) ||
        !near(PointCloudMeasurement::PointToPlaneDistance(cloud.points.front(), plane), 0.0)) {
        return fail("3D point-cloud measurement formulas are incorrect.");
    }

    const std::filesystem::path ply_path =
        std::filesystem::temp_directory_path() / "CameraViewPointCloudTests.ply";
    const std::filesystem::path xyz_path =
        std::filesystem::temp_directory_path() / "CameraViewPointCloudTests.xyz";
    std::wstring error;
    if (!PointCloudIO::SavePly(ply_path, cloud, error) ||
        !PointCloudIO::SaveXyz(xyz_path, cloud, error)) {
        return fail("Point-cloud export failed.");
    }
    PointCloud loaded_ply;
    PointCloud loaded_xyz;
    if (!PointCloudIO::Load(ply_path, loaded_ply, error, PointCloudUnit::Millimeters) ||
        !PointCloudIO::Load(xyz_path, loaded_xyz, error, PointCloudUnit::Millimeters) ||
        loaded_ply.Size() != cloud.Size() || loaded_xyz.Size() != cloud.Size() ||
        !loaded_ply.points.front().has_color || !loaded_xyz.points.front().has_color ||
        !near(loaded_ply.points[100].z, cloud.points[100].z)) {
        return fail("PLY/XYZ round trip did not preserve point data.");
    }
    std::filesystem::remove(ply_path);
    std::filesystem::remove(xyz_path);

    const std::filesystem::path csv_path =
        std::filesystem::temp_directory_path() / "CameraViewPointCloudTests.csv";
    {
        std::ofstream csv(csv_path);
        csv << "x,y,z,r,g,b\n0,1,2,10,20,30\n3,4,5,40,50,60\n";
    }
    PointCloud loaded_csv;
    std::uint64_t last_progress = 0;
    std::uint64_t progress_total = 0;
    int progress_calls = 0;
    if (!PointCloudIO::Load(csv_path, loaded_csv, error, PointCloudUnit::Micrometers,
            [&](std::uint64_t bytes, std::uint64_t total) {
                last_progress = bytes;
                progress_total = total;
                ++progress_calls;
                return true;
            }) ||
        loaded_csv.Size() != 2 || loaded_csv.points[1].g != 50 ||
        loaded_csv.unit != PointCloudUnit::Micrometers || progress_calls < 2 ||
        progress_total == 0 || last_progress != progress_total) {
        return fail("CSV point-cloud import failed.");
    }
    PointCloud cancelled_cloud;
    if (PointCloudIO::Load(csv_path, cancelled_cloud, error, PointCloudUnit::Unknown,
            [](std::uint64_t, std::uint64_t) { return false; }) ||
        !cancelled_cloud.Empty() || error != L"Point-cloud import was cancelled.") {
        return fail("Point-cloud import cancellation was not honored.");
    }
    std::filesystem::remove(csv_path);
    const std::filesystem::path crlf_ply_path =
        std::filesystem::temp_directory_path() / "CameraViewPointCloudTestsCrlf.ply";
    {
        std::ofstream ply(crlf_ply_path, std::ios::binary);
        ply << "ply\r\nformat ascii 1.0\r\nelement vertex 1\r\n"
               "property float x\r\nproperty float y\r\nproperty float z\r\n"
               "end_header\r\n1 2 3\r\n";
    }
    PointCloud crlf_cloud;
    if (!PointCloudIO::Load(crlf_ply_path, crlf_cloud, error) || crlf_cloud.Size() != 1 ||
        !near(crlf_cloud.points.front().z, 3.0)) {
        return fail("CRLF ASCII PLY import failed.");
    }
    std::filesystem::remove(crlf_ply_path);
    const std::filesystem::path pcd_path =
        std::filesystem::temp_directory_path() / "CameraViewPointCloudTests.pcd";
    {
        std::ofstream pcd(pcd_path);
        pcd << "# .PCD v0.7\nVERSION 0.7\nFIELDS x y z r g b\n"
               "SIZE 4 4 4 1 1 1\nTYPE F F F U U U\nCOUNT 1 1 1 1 1 1\n"
               "WIDTH 2\nHEIGHT 1\nPOINTS 2\nDATA ascii\n"
               "1 2 3 10 20 30\n4 5 6 40 50 60\n";
    }
    PointCloud pcd_cloud;
    if (!PointCloudIO::Load(pcd_path, pcd_cloud, error, PointCloudUnit::Meters) ||
        pcd_cloud.Size() != 2 || pcd_cloud.points[1].b != 60 ||
        pcd_cloud.unit != PointCloudUnit::Meters) {
        return fail("ASCII PCD point-cloud import failed.");
    }
    std::filesystem::remove(pcd_path);
    return 0;
}
