#include "ImageExporter.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <utility>
#include <vector>

namespace {

struct BgrColor {
    unsigned char b = 0;
    unsigned char g = 0;
    unsigned char r = 0;
};

int SaveStride(int width)
{
    return (width * 3 + 3) & ~3;
}

int BmpStride(int width, int bits_per_pixel)
{
    return ((width * bits_per_pixel + 31) / 32) * 4;
}

void PutPixel(ImageFrame& frame, int x, int y, BgrColor color)
{
    if (x < 0 || y < 0 || x >= frame.width || y >= frame.height) {
        return;
    }

    unsigned char* pixel = frame.bgr.data() +
        static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.stride) +
        static_cast<std::size_t>(x) * 3U;
    pixel[0] = color.b;
    pixel[1] = color.g;
    pixel[2] = color.r;
}

void DrawDisk(ImageFrame& frame, int center_x, int center_y, int radius, BgrColor color)
{
    for (int y = center_y - radius; y <= center_y + radius; ++y) {
        for (int x = center_x - radius; x <= center_x + radius; ++x) {
            const int dx = x - center_x;
            const int dy = y - center_y;
            if (dx * dx + dy * dy <= radius * radius) {
                PutPixel(frame, x, y, color);
            }
        }
    }
}

void DrawLine(ImageFrame& frame, int x0, int y0, int x1, int y1, int thickness, BgrColor color)
{
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    for (;;) {
        DrawDisk(frame, x0, y0, thickness, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int twice_error = 2 * error;
        if (twice_error >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twice_error <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

void DrawRectangle(ImageFrame& frame, int x0, int y0, int x1, int y1, int thickness, BgrColor color)
{
    DrawLine(frame, x0, y0, x1, y0, thickness, color);
    DrawLine(frame, x1, y0, x1, y1, thickness, color);
    DrawLine(frame, x1, y1, x0, y1, thickness, color);
    DrawLine(frame, x0, y1, x0, y0, thickness, color);
}

void DrawPolygon(ImageFrame& frame, const std::vector<ImagePoint>& points, int thickness, BgrColor color)
{
    if (points.size() < 2) {
        return;
    }

    for (std::size_t index = 0; index < points.size(); ++index) {
        const ImagePoint& first = points[index];
        const ImagePoint& second = points[(index + 1) % points.size()];
        const int x0 = static_cast<int>(std::round(first.x));
        const int y0 = static_cast<int>(std::round(first.y));
        const int x1 = static_cast<int>(std::round(second.x));
        const int y1 = static_cast<int>(std::round(second.y));
        DrawLine(frame, x0, y0, x1, y1, thickness, color);
        DrawDisk(frame, x0, y0, 5, color);
    }
}

ImageFrame CopyForExport(const ImageFrame& source)
{
    ImageFrame output;
    output.width = source.width;
    output.height = source.height;
    output.stride = SaveStride(source.width);
    output.timestamp = source.timestamp;
    output.sequence = source.sequence;
    output.bgr.assign(static_cast<std::size_t>(output.stride) * static_cast<std::size_t>(output.height), 0);

    const int copy_width = std::min(source.stride, output.stride);
    for (int y = 0; y < output.height; ++y) {
        const unsigned char* src = source.bgr.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(source.stride);
        unsigned char* dst = output.bgr.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(output.stride);
        std::copy(src, src + copy_width, dst);
    }

    return output;
}

void WriteU16(std::ofstream& output, std::uint16_t value)
{
    output.put(static_cast<char>(value & 0xFF));
    output.put(static_cast<char>((value >> 8) & 0xFF));
}

void WriteU32(std::ofstream& output, std::uint32_t value)
{
    output.put(static_cast<char>(value & 0xFF));
    output.put(static_cast<char>((value >> 8) & 0xFF));
    output.put(static_cast<char>((value >> 16) & 0xFF));
    output.put(static_cast<char>((value >> 24) & 0xFF));
}

void WriteI32(std::ofstream& output, std::int32_t value)
{
    WriteU32(output, static_cast<std::uint32_t>(value));
}

bool ReadU16(std::ifstream& input, std::uint16_t& value)
{
    unsigned char bytes[2] = {};
    if (!input.read(reinterpret_cast<char*>(bytes), sizeof(bytes))) {
        return false;
    }
    value = static_cast<std::uint16_t>(bytes[0]) |
        (static_cast<std::uint16_t>(bytes[1]) << 8);
    return true;
}

bool ReadU32(std::ifstream& input, std::uint32_t& value)
{
    unsigned char bytes[4] = {};
    if (!input.read(reinterpret_cast<char*>(bytes), sizeof(bytes))) {
        return false;
    }
    value = static_cast<std::uint32_t>(bytes[0]) |
        (static_cast<std::uint32_t>(bytes[1]) << 8) |
        (static_cast<std::uint32_t>(bytes[2]) << 16) |
        (static_cast<std::uint32_t>(bytes[3]) << 24);
    return true;
}

bool ReadI32(std::ifstream& input, std::int32_t& value)
{
    std::uint32_t raw = 0;
    if (!ReadU32(input, raw)) {
        return false;
    }
    value = static_cast<std::int32_t>(raw);
    return true;
}

} // namespace

bool ImageExporter::LoadBmp(
    const std::filesystem::path& path,
    ImageFrame& frame,
    std::wstring& error)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = L"Failed to open image file.";
        return false;
    }

    std::uint16_t signature = 0;
    std::uint32_t file_size = 0;
    std::uint16_t reserved = 0;
    std::uint32_t pixel_offset = 0;
    std::uint32_t dib_size = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::uint16_t planes = 0;
    std::uint16_t bits_per_pixel = 0;
    std::uint32_t compression = 0;
    std::uint32_t image_size = 0;
    std::uint32_t colors_used = 0;
    std::uint32_t ignored = 0;

    if (!ReadU16(input, signature) ||
        !ReadU32(input, file_size) ||
        !ReadU16(input, reserved) ||
        !ReadU16(input, reserved) ||
        !ReadU32(input, pixel_offset) ||
        !ReadU32(input, dib_size) ||
        !ReadI32(input, width) ||
        !ReadI32(input, height) ||
        !ReadU16(input, planes) ||
        !ReadU16(input, bits_per_pixel) ||
        !ReadU32(input, compression) ||
        !ReadU32(input, image_size) ||
        !ReadU32(input, ignored) ||
        !ReadU32(input, ignored) ||
        !ReadU32(input, colors_used) ||
        !ReadU32(input, ignored)) {
        error = L"Invalid BMP file.";
        return false;
    }

    if (signature != 0x4D42 || dib_size < 40 || width <= 0 || height == 0) {
        error = L"Invalid BMP file.";
        return false;
    }
    if (planes != 1 || compression != 0 || (bits_per_pixel != 8 && bits_per_pixel != 24 && bits_per_pixel != 32)) {
        error = L"Only uncompressed 8-bit, 24-bit, or 32-bit BMP images are supported.";
        return false;
    }

    const int output_width = width;
    const int output_height = height < 0 ? -height : height;
    const int output_stride = SaveStride(output_width);
    const int input_stride = BmpStride(output_width, bits_per_pixel);
    ImageFrame loaded;
    loaded.width = output_width;
    loaded.height = output_height;
    loaded.stride = output_stride;
    loaded.bgr.assign(
        static_cast<std::size_t>(loaded.stride) * static_cast<std::size_t>(loaded.height),
        0);

    std::vector<BgrColor> palette;
    if (bits_per_pixel == 8) {
        const std::streamoff palette_start = 14 + static_cast<std::streamoff>(dib_size);
        if (static_cast<std::streamoff>(pixel_offset) < palette_start) {
            error = L"Invalid BMP palette.";
            return false;
        }

        const std::streamoff available_entries =
            (static_cast<std::streamoff>(pixel_offset) - palette_start) / 4;
        std::uint32_t palette_entries =
            static_cast<std::uint32_t>(std::min<std::streamoff>(available_entries, 256));
        if (colors_used != 0) {
            palette_entries = std::min<std::uint32_t>(palette_entries, colors_used);
        }

        input.seekg(palette_start, std::ios::beg);
        if (!input) {
            error = L"Invalid BMP palette.";
            return false;
        }

        palette.reserve(palette_entries);
        for (std::uint32_t index = 0; index < palette_entries; ++index) {
            unsigned char entry[4] = {};
            if (!input.read(reinterpret_cast<char*>(entry), sizeof(entry))) {
                error = L"Invalid BMP palette.";
                return false;
            }
            palette.push_back(BgrColor{entry[0], entry[1], entry[2]});
        }
    }

    input.seekg(static_cast<std::streamoff>(pixel_offset), std::ios::beg);
    if (!input) {
        error = L"Invalid BMP pixel data.";
        return false;
    }

    std::vector<unsigned char> row(static_cast<std::size_t>(input_stride), 0);
    for (int file_y = 0; file_y < output_height; ++file_y) {
        if (!input.read(reinterpret_cast<char*>(row.data()), static_cast<std::streamsize>(row.size()))) {
            error = L"Invalid BMP pixel data.";
            return false;
        }
        const int target_y = height > 0 ? output_height - 1 - file_y : file_y;
        unsigned char* dst = loaded.bgr.data() +
            static_cast<std::size_t>(target_y) * static_cast<std::size_t>(loaded.stride);
        if (bits_per_pixel == 24) {
            std::copy(row.begin(), row.begin() + output_width * 3, dst);
        } else if (bits_per_pixel == 32) {
            for (int x = 0; x < output_width; ++x) {
                const std::size_t source_offset = static_cast<std::size_t>(x) * 4U;
                dst[x * 3 + 0] = row[source_offset + 0];
                dst[x * 3 + 1] = row[source_offset + 1];
                dst[x * 3 + 2] = row[source_offset + 2];
            }
        } else {
            for (int x = 0; x < output_width; ++x) {
                const unsigned char value = row[static_cast<std::size_t>(x)];
                const BgrColor color = value < palette.size()
                    ? palette[value]
                    : BgrColor{value, value, value};
                dst[x * 3 + 0] = color.b;
                dst[x * 3 + 1] = color.g;
                dst[x * 3 + 2] = color.r;
            }
        }
    }

    frame = std::move(loaded);
    error.clear();
    return true;
}

bool ImageExporter::SaveBmp(
    const std::filesystem::path& path,
    const ImageFrame& frame,
    const std::vector<LengthMeasurement>& measurements,
    std::wstring& error)
{
    return SaveBmp(path, frame, measurements, {}, {}, {}, error);
}

bool ImageExporter::SaveBmp(
    const std::filesystem::path& path,
    const ImageFrame& frame,
    const std::vector<LengthMeasurement>& length_measurements,
    const std::vector<AngleMeasurement>& angle_measurements,
    const std::vector<RectangleAreaMeasurement>& rectangle_measurements,
    const std::vector<PolygonAreaMeasurement>& polygon_measurements,
    std::wstring& error)
{
    if (!frame.IsValid()) {
        error = L"No image frame to export.";
        return false;
    }

    ImageFrame output_frame = CopyForExport(frame);

    const BgrColor measurement_color{74, 214, 255};
    for (const LengthMeasurement& measurement : length_measurements) {
        const ImagePoint first = measurement.First();
        const ImagePoint second = measurement.Second();
        const int x0 = static_cast<int>(std::round(first.x));
        const int y0 = static_cast<int>(std::round(first.y));
        const int x1 = static_cast<int>(std::round(second.x));
        const int y1 = static_cast<int>(std::round(second.y));
        DrawLine(output_frame, x0, y0, x1, y1, 2, measurement_color);
        DrawDisk(output_frame, x0, y0, 5, measurement_color);
        DrawDisk(output_frame, x1, y1, 5, measurement_color);
    }
    for (const AngleMeasurement& measurement : angle_measurements) {
        const ImagePoint first = measurement.First();
        const ImagePoint vertex = measurement.Vertex();
        const ImagePoint second = measurement.Second();
        const int x0 = static_cast<int>(std::round(first.x));
        const int y0 = static_cast<int>(std::round(first.y));
        const int vx = static_cast<int>(std::round(vertex.x));
        const int vy = static_cast<int>(std::round(vertex.y));
        const int x1 = static_cast<int>(std::round(second.x));
        const int y1 = static_cast<int>(std::round(second.y));
        DrawLine(output_frame, vx, vy, x0, y0, 2, measurement_color);
        DrawLine(output_frame, vx, vy, x1, y1, 2, measurement_color);
        DrawDisk(output_frame, x0, y0, 5, measurement_color);
        DrawDisk(output_frame, vx, vy, 5, measurement_color);
        DrawDisk(output_frame, x1, y1, 5, measurement_color);
    }
    for (const RectangleAreaMeasurement& measurement : rectangle_measurements) {
        const ImagePoint first = measurement.First();
        const ImagePoint second = measurement.Second();
        const int x0 = static_cast<int>(std::round(first.x));
        const int y0 = static_cast<int>(std::round(first.y));
        const int x1 = static_cast<int>(std::round(second.x));
        const int y1 = static_cast<int>(std::round(second.y));
        DrawRectangle(output_frame, x0, y0, x1, y1, 2, measurement_color);
        DrawDisk(output_frame, x0, y0, 5, measurement_color);
        DrawDisk(output_frame, x1, y1, 5, measurement_color);
    }
    for (const PolygonAreaMeasurement& measurement : polygon_measurements) {
        DrawPolygon(output_frame, measurement.Points(), 2, measurement_color);
    }

    const std::uint32_t header_size = 14U + 40U;
    const std::uint32_t image_size =
        static_cast<std::uint32_t>(output_frame.stride) * static_cast<std::uint32_t>(output_frame.height);
    const std::uint32_t file_size = header_size + image_size;

    std::ofstream output(path, std::ios::binary);
    if (!output) {
        error = L"Failed to create image file.";
        return false;
    }

    output.put('B');
    output.put('M');
    WriteU32(output, file_size);
    WriteU16(output, 0);
    WriteU16(output, 0);
    WriteU32(output, header_size);

    WriteU32(output, 40);
    WriteI32(output, output_frame.width);
    WriteI32(output, output_frame.height);
    WriteU16(output, 1);
    WriteU16(output, 24);
    WriteU32(output, 0);
    WriteU32(output, image_size);
    WriteI32(output, 2835);
    WriteI32(output, 2835);
    WriteU32(output, 0);
    WriteU32(output, 0);

    for (int y = output_frame.height - 1; y >= 0; --y) {
        const unsigned char* row = output_frame.bgr.data() +
            static_cast<std::size_t>(y) * static_cast<std::size_t>(output_frame.stride);
        output.write(reinterpret_cast<const char*>(row), output_frame.stride);
    }

    if (!output) {
        error = L"Failed while writing image file.";
        return false;
    }

    error.clear();
    return true;
}
