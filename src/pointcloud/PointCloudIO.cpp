#include "PointCloudIO.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

namespace {

class LoadProgressTracker final {
public:
    LoadProgressTracker(
        PointCloudIO::LoadProgress progress,
        std::uint64_t total_bytes,
        std::wstring& error)
        : progress_(std::move(progress)), total_bytes_(total_bytes), error_(error)
    {
    }

    bool Report(std::ifstream& input, bool force = false)
    {
        if (!progress_) return true;
        ++poll_count_;
        if (!force && poll_count_ % 256 != 0) return true;
        const std::streampos position = input.tellg();
        const std::uint64_t bytes_read = position < std::streampos(0)
            ? total_bytes_
            : static_cast<std::uint64_t>(position);
        if (progress_(std::min(bytes_read, total_bytes_), total_bytes_)) return true;
        error_ = L"Point-cloud import was cancelled.";
        return false;
    }

private:
    PointCloudIO::LoadProgress progress_;
    std::uint64_t total_bytes_ = 0;
    std::uint64_t poll_count_ = 0;
    std::wstring& error_;
};

std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return value;
}

std::string Trim(std::string value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char character) {
        return std::isspace(character) != 0;
    }).base();
    return first < last ? std::string(first, last) : std::string();
}

bool ParseNumberRow(std::string line, PointCloudPoint& point)
{
    const std::size_t comment = line.find('#');
    if (comment != std::string::npos) line.erase(comment);
    for (char& character : line) {
        if (character == ',' || character == ';' || character == '\t') character = ' ';
    }
    std::istringstream input(line);
    if (!(input >> point.x >> point.y >> point.z)) return false;
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {
        return false;
    }
    int red = 0;
    int green = 0;
    int blue = 0;
    if (input >> red >> green >> blue) {
        point.r = static_cast<std::uint8_t>(std::clamp(red, 0, 255));
        point.g = static_cast<std::uint8_t>(std::clamp(green, 0, 255));
        point.b = static_cast<std::uint8_t>(std::clamp(blue, 0, 255));
        point.has_color = true;
    }
    return true;
}

bool LoadDelimited(
    std::ifstream& input,
    PointCloud& cloud,
    std::wstring& error,
    LoadProgressTracker& progress)
{
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        if (!progress.Report(input)) return false;
        ++line_number;
        PointCloudPoint point;
        if (ParseNumberRow(line, point)) {
            cloud.points.push_back(point);
            continue;
        }
        const bool blank = std::all_of(line.begin(), line.end(), [](unsigned char value) {
            return std::isspace(value) != 0;
        });
        const bool comment = !line.empty() && line.front() == '#';
        const bool header = line_number == 1 &&
            (Lower(line).find('x') != std::string::npos || Lower(line).find("point") != std::string::npos);
        if (!blank && !comment && !header) {
            error = L"Invalid point row at line " + std::to_wstring(line_number) + L".";
            return false;
        }
    }
    if (cloud.points.empty()) {
        error = L"The point-cloud file does not contain any XYZ points.";
        return false;
    }
    return true;
}

bool LoadAsciiPly(
    std::ifstream& input,
    PointCloud& cloud,
    std::wstring& error,
    LoadProgressTracker& progress)
{
    std::string line;
    if (!std::getline(input, line) || Lower(Trim(line)) != "ply") {
        error = L"The file is not a valid PLY document.";
        return false;
    }
    std::size_t vertex_count = 0;
    bool ascii = false;
    bool in_vertex_element = false;
    std::vector<std::string> properties;
    while (std::getline(input, line)) {
        if (!progress.Report(input)) return false;
        std::istringstream header(line);
        std::string keyword;
        header >> keyword;
        keyword = Lower(keyword);
        if (keyword == "format") {
            std::string format;
            header >> format;
            ascii = Lower(format) == "ascii";
        } else if (keyword == "element") {
            std::string element;
            std::size_t count = 0;
            header >> element >> count;
            in_vertex_element = Lower(element) == "vertex";
            if (in_vertex_element) vertex_count = count;
        } else if (keyword == "property" && in_vertex_element) {
            std::string type;
            std::string name;
            header >> type >> name;
            properties.push_back(Lower(name));
        } else if (keyword == "end_header") {
            break;
        }
    }
    if (!ascii) {
        error = L"Only ASCII PLY point clouds are supported.";
        return false;
    }
    const auto propertyIndex = [&properties](const char* name) {
        const auto match = std::find(properties.begin(), properties.end(), name);
        return match == properties.end() ? -1 : static_cast<int>(match - properties.begin());
    };
    const int x_index = propertyIndex("x");
    const int y_index = propertyIndex("y");
    const int z_index = propertyIndex("z");
    int r_index = propertyIndex("red");
    int g_index = propertyIndex("green");
    int b_index = propertyIndex("blue");
    if (r_index < 0) r_index = propertyIndex("r");
    if (g_index < 0) g_index = propertyIndex("g");
    if (b_index < 0) b_index = propertyIndex("b");
    if (vertex_count == 0 || x_index < 0 || y_index < 0 || z_index < 0) {
        error = L"PLY header is missing a vertex count or XYZ properties.";
        return false;
    }
    cloud.points.reserve(vertex_count);
    for (std::size_t index = 0; index < vertex_count && std::getline(input, line); ++index) {
        if (!progress.Report(input)) return false;
        std::istringstream row(line);
        std::vector<double> values;
        double value = 0.0;
        while (row >> value) values.push_back(value);
        const int required = std::max({x_index, y_index, z_index, r_index, g_index, b_index});
        if (values.size() <= static_cast<std::size_t>(std::max(required, 0))) {
            error = L"Invalid PLY vertex row at index " + std::to_wstring(index) + L".";
            return false;
        }
        PointCloudPoint point;
        point.x = values[static_cast<std::size_t>(x_index)];
        point.y = values[static_cast<std::size_t>(y_index)];
        point.z = values[static_cast<std::size_t>(z_index)];
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {
            error = L"PLY contains a non-finite vertex coordinate.";
            return false;
        }
        if (r_index >= 0 && g_index >= 0 && b_index >= 0) {
            point.r = static_cast<std::uint8_t>(std::clamp(values[r_index], 0.0, 255.0));
            point.g = static_cast<std::uint8_t>(std::clamp(values[g_index], 0.0, 255.0));
            point.b = static_cast<std::uint8_t>(std::clamp(values[b_index], 0.0, 255.0));
            point.has_color = true;
        }
        cloud.points.push_back(point);
    }
    if (cloud.points.size() != vertex_count) {
        error = L"PLY ended before all declared vertices were read.";
        return false;
    }
    return true;
}

bool LoadAsciiPcd(
    std::ifstream& input,
    PointCloud& cloud,
    std::wstring& error,
    LoadProgressTracker& progress)
{
    std::string line;
    std::vector<std::string> fields;
    std::size_t declared_points = 0;
    bool ascii = false;
    while (std::getline(input, line)) {
        if (!progress.Report(input)) return false;
        std::istringstream header(line);
        std::string keyword;
        header >> keyword;
        keyword = Lower(keyword);
        if (keyword == "fields") {
            std::string field;
            while (header >> field) fields.push_back(Lower(field));
        } else if (keyword == "points") {
            header >> declared_points;
        } else if (keyword == "data") {
            std::string format;
            header >> format;
            ascii = Lower(format) == "ascii";
            break;
        }
    }
    if (!ascii) {
        error = L"Only ASCII PCD point clouds are supported.";
        return false;
    }
    const auto fieldIndex = [&fields](const char* name) {
        const auto match = std::find(fields.begin(), fields.end(), name);
        return match == fields.end() ? -1 : static_cast<int>(match - fields.begin());
    };
    const int x_index = fieldIndex("x");
    const int y_index = fieldIndex("y");
    const int z_index = fieldIndex("z");
    const int r_index = fieldIndex("r");
    const int g_index = fieldIndex("g");
    const int b_index = fieldIndex("b");
    if (x_index < 0 || y_index < 0 || z_index < 0) {
        error = L"PCD header is missing XYZ fields.";
        return false;
    }
    cloud.points.reserve(declared_points);
    while (std::getline(input, line)) {
        if (!progress.Report(input)) return false;
        std::istringstream row(line);
        std::vector<double> values;
        double value = 0.0;
        while (row >> value) values.push_back(value);
        if (values.empty()) continue;
        const int required = std::max({x_index, y_index, z_index, r_index, g_index, b_index});
        if (values.size() <= static_cast<std::size_t>(std::max(required, 0))) {
            error = L"Invalid PCD point row.";
            return false;
        }
        PointCloudPoint point;
        point.x = values[static_cast<std::size_t>(x_index)];
        point.y = values[static_cast<std::size_t>(y_index)];
        point.z = values[static_cast<std::size_t>(z_index)];
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {
            error = L"PCD contains a non-finite point coordinate.";
            return false;
        }
        if (r_index >= 0 && g_index >= 0 && b_index >= 0) {
            point.r = static_cast<std::uint8_t>(std::clamp(values[r_index], 0.0, 255.0));
            point.g = static_cast<std::uint8_t>(std::clamp(values[g_index], 0.0, 255.0));
            point.b = static_cast<std::uint8_t>(std::clamp(values[b_index], 0.0, 255.0));
            point.has_color = true;
        }
        cloud.points.push_back(point);
    }
    if (cloud.points.empty() || (declared_points > 0 && cloud.points.size() != declared_points)) {
        error = L"PCD point count does not match its header.";
        return false;
    }
    return true;
}

} // namespace

bool PointCloudIO::Load(
    const std::filesystem::path& path,
    PointCloud& cloud,
    std::wstring& error,
    PointCloudUnit unit,
    const LoadProgress& progress)
{
    cloud = {};
    error.clear();
    std::ifstream input(path);
    if (!input) {
        error = L"Could not open the point-cloud file.";
        return false;
    }
    std::error_code file_size_error;
    const std::uintmax_t file_size = std::filesystem::file_size(path, file_size_error);
    const std::uint64_t total_bytes = file_size_error
        ? 0
        : static_cast<std::uint64_t>(file_size);
    LoadProgressTracker progress_tracker(progress, total_bytes, error);
    if (!progress_tracker.Report(input, true)) return false;
    const std::string extension = Lower(path.extension().string());
    const bool loaded = extension == ".ply"
        ? LoadAsciiPly(input, cloud, error, progress_tracker)
        : extension == ".pcd" ? LoadAsciiPcd(input, cloud, error, progress_tracker)
                              : LoadDelimited(input, cloud, error, progress_tracker);
    if (!loaded) {
        cloud = {};
        return false;
    }
    cloud.unit = unit;
    cloud.name = path.filename().wstring();
    cloud.source_path = path.wstring();
    cloud.RecalculateBounds();
    if (!progress_tracker.Report(input, true)) {
        cloud = {};
        return false;
    }
    return true;
}

bool PointCloudIO::SavePly(
    const std::filesystem::path& path,
    const PointCloud& cloud,
    std::wstring& error)
{
    error.clear();
    if (cloud.Empty()) {
        error = L"There is no point cloud to export.";
        return false;
    }
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        error = L"Could not create the PLY file.";
        return false;
    }
    const bool has_color = std::any_of(cloud.points.begin(), cloud.points.end(), [](const auto& point) {
        return point.has_color;
    });
    output << "ply\nformat ascii 1.0\ncomment Generated by CameraView\n"
           << "element vertex " << cloud.points.size() << "\n"
           << "property double x\nproperty double y\nproperty double z\n";
    if (has_color) {
        output << "property uchar red\nproperty uchar green\nproperty uchar blue\n";
    }
    output << "end_header\n" << std::setprecision(12);
    for (const PointCloudPoint& point : cloud.points) {
        output << point.x << ' ' << point.y << ' ' << point.z;
        if (has_color) {
            output << ' ' << static_cast<int>(point.r)
                   << ' ' << static_cast<int>(point.g)
                   << ' ' << static_cast<int>(point.b);
        }
        output << '\n';
    }
    if (!output) {
        error = L"Could not finish writing the PLY file.";
        return false;
    }
    return true;
}

bool PointCloudIO::SaveXyz(
    const std::filesystem::path& path,
    const PointCloud& cloud,
    std::wstring& error)
{
    error.clear();
    if (cloud.Empty()) {
        error = L"There is no point cloud to export.";
        return false;
    }
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        error = L"Could not create the XYZ file.";
        return false;
    }
    output << std::setprecision(12);
    for (const PointCloudPoint& point : cloud.points) {
        output << point.x << ' ' << point.y << ' ' << point.z;
        if (point.has_color) {
            output << ' ' << static_cast<int>(point.r)
                   << ' ' << static_cast<int>(point.g)
                   << ' ' << static_cast<int>(point.b);
        }
        output << '\n';
    }
    if (!output) {
        error = L"Could not finish writing the XYZ file.";
        return false;
    }
    return true;
}
