#include "pointcloud/PointCloud.h"
#include "pointcloud/PointCloudIO.h"
#include "pointcloud/PointCloudMeasurement.h"
#include "pointcloud/PointCloudProcessor.h"

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
