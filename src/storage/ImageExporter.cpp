#include "ImageExporter.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include "../imaging/OverlayRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cwctype>
#include <fstream>
#include <utility>
#include <vector>

using Microsoft::WRL::ComPtr;

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

std::wstring PathExtensionLower(const std::filesystem::path& path)
{
    std::wstring extension = path.extension().wstring();
    std::transform(
        extension.begin(),
        extension.end(),
        extension.begin(),
        [](wchar_t value) { return static_cast<wchar_t>(std::towlower(value)); });
    return extension;
}

bool IsBmpPath(const std::filesystem::path& path)
{
    return PathExtensionLower(path) == L".bmp";
}

const GUID* WicContainerFormatForPath(const std::filesystem::path& path)
{
    const std::wstring extension = PathExtensionLower(path);
    if (extension == L".jpg" || extension == L".jpeg") {
        return &GUID_ContainerFormatJpeg;
    }
    if (extension == L".png") {
        return &GUID_ContainerFormatPng;
    }
    if (extension == L".tif" || extension == L".tiff") {
        return &GUID_ContainerFormatTiff;
    }
    return nullptr;
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

void DrawMeasurements(
    ImageFrame& output_frame,
    const std::vector<LengthMeasurement>& length_measurements,
    const std::vector<AngleMeasurement>& angle_measurements,
    const std::vector<RectangleAreaMeasurement>& rectangle_measurements,
    const std::vector<PolygonAreaMeasurement>& polygon_measurements)
{
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
}

void DrawScaleBar(ImageFrame& output_frame, const CalibrationProfile* calibration)
{
    if (!calibration || !calibration->IsCalibrated()) {
        return;
    }

    const ScaleBarOverlay overlay =
        OverlayRenderer::BuildScaleBarOverlay(*calibration, output_frame.width, 1.0);
    if (!overlay.visible || output_frame.width <= 0 || output_frame.height <= 0) {
        return;
    }

    BITMAPINFO bitmap_info = {};
    bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap_info.bmiHeader.biWidth = output_frame.width;
    bitmap_info.bmiHeader.biHeight = -output_frame.height;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 24;
    bitmap_info.bmiHeader.biCompression = BI_RGB;
    bitmap_info.bmiHeader.biSizeImage =
        static_cast<DWORD>(output_frame.stride * output_frame.height);

    HDC screen_dc = GetDC(nullptr);
    if (!screen_dc) {
        return;
    }
    void* bitmap_bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(
        screen_dc,
        &bitmap_info,
        DIB_RGB_COLORS,
        &bitmap_bits,
        nullptr,
        0);
    HDC memory_dc = bitmap ? CreateCompatibleDC(screen_dc) : nullptr;
    ReleaseDC(nullptr, screen_dc);
    if (!bitmap || !memory_dc || !bitmap_bits) {
        if (memory_dc) {
            DeleteDC(memory_dc);
        }
        if (bitmap) {
            DeleteObject(bitmap);
        }
        return;
    }

    std::copy(
        output_frame.bgr.begin(),
        output_frame.bgr.end(),
        static_cast<unsigned char*>(bitmap_bits));

    HGDIOBJ old_bitmap = SelectObject(memory_dc, bitmap);
    const int margin = std::clamp(std::min(output_frame.width, output_frame.height) / 8, 12, 24);
    const int tick = std::clamp(output_frame.height / 32, 5, 9);
    const int x1 = output_frame.width - margin;
    const int x0 = x1 - overlay.screen_length;
    const int y = output_frame.height - margin;
    if (x0 >= margin && y > tick) {
        const int font_height = std::clamp(output_frame.height / 20, 14, 26);
        HPEN shadow_pen = CreatePen(PS_SOLID, 6, RGB(0, 0, 0));
        HPEN bar_pen = CreatePen(PS_SOLID, 3, RGB(255, 255, 255));
        HFONT font = CreateFontW(
            font_height,
            0,
            0,
            0,
            FW_SEMIBOLD,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS,
            L"Segoe UI");

        HGDIOBJ old_pen = shadow_pen ? SelectObject(memory_dc, shadow_pen) : nullptr;
        HGDIOBJ old_font = font ? SelectObject(memory_dc, font) : nullptr;
        SetBkMode(memory_dc, TRANSPARENT);
        MoveToEx(memory_dc, x0, y, nullptr);
        LineTo(memory_dc, x1, y);
        MoveToEx(memory_dc, x0, y - tick, nullptr);
        LineTo(memory_dc, x0, y + tick);
        MoveToEx(memory_dc, x1, y - tick, nullptr);
        LineTo(memory_dc, x1, y + tick);

        if (bar_pen) {
            SelectObject(memory_dc, bar_pen);
        }
        MoveToEx(memory_dc, x0, y, nullptr);
        LineTo(memory_dc, x1, y);
        MoveToEx(memory_dc, x0, y - tick, nullptr);
        LineTo(memory_dc, x0, y + tick);
        MoveToEx(memory_dc, x1, y - tick, nullptr);
        LineTo(memory_dc, x1, y + tick);

        RECT label_rect{x0 - 40, y - font_height - 10, x1 + 2, y - 8};
        RECT shadow_rect = label_rect;
        OffsetRect(&shadow_rect, 1, 1);
        SetTextColor(memory_dc, RGB(0, 0, 0));
        DrawTextW(memory_dc, overlay.label.c_str(), -1, &shadow_rect, DT_RIGHT | DT_SINGLELINE | DT_END_ELLIPSIS);
        SetTextColor(memory_dc, RGB(255, 255, 255));
        DrawTextW(memory_dc, overlay.label.c_str(), -1, &label_rect, DT_RIGHT | DT_SINGLELINE | DT_END_ELLIPSIS);

        if (old_font) {
            SelectObject(memory_dc, old_font);
        }
        if (old_pen) {
            SelectObject(memory_dc, old_pen);
        }
        if (font) {
            DeleteObject(font);
        }
        if (bar_pen) {
            DeleteObject(bar_pen);
        }
        if (shadow_pen) {
            DeleteObject(shadow_pen);
        }
    }

    GdiFlush();
    std::copy(
        static_cast<unsigned char*>(bitmap_bits),
        static_cast<unsigned char*>(bitmap_bits) + output_frame.bgr.size(),
        output_frame.bgr.begin());

    SelectObject(memory_dc, old_bitmap);
    DeleteDC(memory_dc);
    DeleteObject(bitmap);
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

class ScopedComInitializer {
public:
    ScopedComInitializer()
    {
        const HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        initialized_ = result == S_OK || result == S_FALSE;
        failed_ = FAILED(result) && result != RPC_E_CHANGED_MODE;
    }

    ~ScopedComInitializer()
    {
        if (initialized_) {
            CoUninitialize();
        }
    }

    bool Failed() const
    {
        return failed_;
    }

private:
    bool initialized_ = false;
    bool failed_ = false;
};

bool LoadWicGray16Frame(
    IWICBitmapFrameDecode* source_frame,
    ImageFrame& frame,
    std::wstring& error)
{
    UINT width = 0;
    UINT height = 0;
    HRESULT result = source_frame->GetSize(&width, &height);
    if (FAILED(result) || width == 0 || height == 0 || width > 0x7FFFFFFFU || height > 0x7FFFFFFFU) {
        error = L"Invalid image size.";
        return false;
    }

    const std::size_t raw_stride = static_cast<std::size_t>(width) * sizeof(std::uint16_t);
    const std::size_t raw_size = raw_stride * static_cast<std::size_t>(height);
    if (raw_size > static_cast<std::size_t>(0xFFFFFFFFU)) {
        error = L"Image is too large to open.";
        return false;
    }

    std::vector<std::uint16_t> gray16(raw_size / sizeof(std::uint16_t), 0);
    result = source_frame->CopyPixels(
        nullptr,
        static_cast<UINT>(raw_stride),
        static_cast<UINT>(raw_size),
        reinterpret_cast<BYTE*>(gray16.data()));
    if (FAILED(result)) {
        error = L"Failed to read image pixels.";
        return false;
    }

    std::uint16_t min_value = 0xFFFFU;
    std::uint16_t max_value = 0;
    for (std::uint16_t value : gray16) {
        min_value = std::min(min_value, value);
        max_value = std::max(max_value, value);
    }

    ImageFrame loaded;
    loaded.width = static_cast<int>(width);
    loaded.height = static_cast<int>(height);
    loaded.stride = SaveStride(loaded.width);
    loaded.bgr.assign(
        static_cast<std::size_t>(loaded.stride) * static_cast<std::size_t>(loaded.height),
        0);

    const std::uint32_t range = static_cast<std::uint32_t>(max_value) - static_cast<std::uint32_t>(min_value);
    for (int y = 0; y < loaded.height; ++y) {
        unsigned char* dst = loaded.bgr.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(loaded.stride);
        const std::uint16_t* src = gray16.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width);
        for (int x = 0; x < loaded.width; ++x) {
            const std::uint16_t source_value = src[x];
            const unsigned char gray = range == 0
                ? static_cast<unsigned char>(source_value >> 8)
                : static_cast<unsigned char>(
                    ((static_cast<std::uint32_t>(source_value) - static_cast<std::uint32_t>(min_value)) * 255U +
                     range / 2U) /
                    range);
            dst[x * 3 + 0] = gray;
            dst[x * 3 + 1] = gray;
            dst[x * 3 + 2] = gray;
        }
    }

    frame = std::move(loaded);
    error.clear();
    return true;
}

bool LoadWicImage(
    const std::filesystem::path& path,
    ImageFrame& frame,
    std::wstring& error)
{
    ScopedComInitializer com;
    if (com.Failed()) {
        error = L"Failed to initialize image decoder.";
        return false;
    }

    ComPtr<IWICImagingFactory> factory;
    HRESULT result = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (FAILED(result)) {
        error = L"Failed to initialize image decoder.";
        return false;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    result = factory->CreateDecoderFromFilename(
        path.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        &decoder);
    if (FAILED(result)) {
        error = L"Failed to open image file.";
        return false;
    }

    ComPtr<IWICBitmapFrameDecode> source_frame;
    result = decoder->GetFrame(0, &source_frame);
    if (FAILED(result)) {
        error = L"Failed to decode image frame.";
        return false;
    }

    WICPixelFormatGUID source_pixel_format = {};
    result = source_frame->GetPixelFormat(&source_pixel_format);
    if (SUCCEEDED(result) && IsEqualGUID(source_pixel_format, GUID_WICPixelFormat16bppGray)) {
        return LoadWicGray16Frame(source_frame.Get(), frame, error);
    }

    ComPtr<IWICFormatConverter> converter;
    result = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(result)) {
        result = converter->Initialize(
            source_frame.Get(),
            GUID_WICPixelFormat24bppBGR,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom);
    }
    if (FAILED(result)) {
        error = L"Unsupported image pixel format.";
        return false;
    }

    UINT width = 0;
    UINT height = 0;
    result = converter->GetSize(&width, &height);
    if (FAILED(result) || width == 0 || height == 0 || width > 0x7FFFFFFFU || height > 0x7FFFFFFFU) {
        error = L"Invalid image size.";
        return false;
    }

    ImageFrame loaded;
    loaded.width = static_cast<int>(width);
    loaded.height = static_cast<int>(height);
    loaded.stride = SaveStride(loaded.width);
    loaded.bgr.assign(
        static_cast<std::size_t>(loaded.stride) * static_cast<std::size_t>(loaded.height),
        0);

    result = converter->CopyPixels(
        nullptr,
        static_cast<UINT>(loaded.stride),
        static_cast<UINT>(loaded.bgr.size()),
        loaded.bgr.data());
    if (FAILED(result)) {
        error = L"Failed to read image pixels.";
        return false;
    }

    frame = std::move(loaded);
    error.clear();
    return true;
}

bool SaveWicImage(
    const std::filesystem::path& path,
    const ImageFrame& frame,
    REFGUID container_format,
    std::wstring& error)
{
    if (frame.bgr.size() > static_cast<std::size_t>(0xFFFFFFFFU)) {
        error = L"Image is too large to export.";
        return false;
    }

    ScopedComInitializer com;
    if (com.Failed()) {
        error = L"Failed to initialize image encoder.";
        return false;
    }

    ComPtr<IWICImagingFactory> factory;
    HRESULT result = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (FAILED(result)) {
        error = L"Failed to initialize image encoder.";
        return false;
    }

    ComPtr<IWICStream> stream;
    result = factory->CreateStream(&stream);
    const std::wstring file_name = path.wstring();
    if (SUCCEEDED(result)) {
        result = stream->InitializeFromFilename(file_name.c_str(), GENERIC_WRITE);
    }
    if (FAILED(result)) {
        error = L"Failed to create image file.";
        return false;
    }

    ComPtr<IWICBitmapEncoder> encoder;
    result = factory->CreateEncoder(container_format, nullptr, &encoder);
    if (SUCCEEDED(result)) {
        result = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    }
    if (FAILED(result)) {
        error = L"Failed to initialize image encoder.";
        return false;
    }

    ComPtr<IWICBitmapFrameEncode> frame_encoder;
    result = encoder->CreateNewFrame(&frame_encoder, nullptr);
    if (SUCCEEDED(result)) {
        result = frame_encoder->Initialize(nullptr);
    }
    if (SUCCEEDED(result)) {
        result = frame_encoder->SetSize(
            static_cast<UINT>(frame.width),
            static_cast<UINT>(frame.height));
    }

    WICPixelFormatGUID pixel_format = GUID_WICPixelFormat24bppBGR;
    if (SUCCEEDED(result)) {
        result = frame_encoder->SetPixelFormat(&pixel_format);
    }
    if (SUCCEEDED(result) && !IsEqualGUID(pixel_format, GUID_WICPixelFormat24bppBGR)) {
        result = E_FAIL;
    }
    if (FAILED(result)) {
        error = L"Unsupported image export format.";
        return false;
    }

    result = frame_encoder->WritePixels(
        static_cast<UINT>(frame.height),
        static_cast<UINT>(frame.stride),
        static_cast<UINT>(frame.bgr.size()),
        const_cast<BYTE*>(frame.bgr.data()));
    if (SUCCEEDED(result)) {
        result = frame_encoder->Commit();
    }
    if (SUCCEEDED(result)) {
        result = encoder->Commit();
    }
    if (FAILED(result)) {
        error = L"Failed while writing image file.";
        return false;
    }

    error.clear();
    return true;
}

} // namespace

bool ImageExporter::LoadRasterImage(
    const std::filesystem::path& path,
    ImageFrame& frame,
    std::wstring& error)
{
    if (IsBmpPath(path)) {
        std::wstring bmp_error;
        if (LoadBmp(path, frame, bmp_error)) {
            error.clear();
            return true;
        }

        std::wstring wic_error;
        if (LoadWicImage(path, frame, wic_error)) {
            error.clear();
            return true;
        }

        error = !bmp_error.empty() ? bmp_error : wic_error;
        return false;
    }

    return LoadWicImage(path, frame, error);
}

bool ImageExporter::SaveRasterImage(
    const std::filesystem::path& path,
    const ImageFrame& frame,
    const std::vector<LengthMeasurement>& measurements,
    std::wstring& error,
    const CalibrationProfile* calibration)
{
    return SaveRasterImage(path, frame, measurements, {}, {}, {}, error, calibration);
}

bool ImageExporter::SaveRasterImage(
    const std::filesystem::path& path,
    const ImageFrame& frame,
    const std::vector<LengthMeasurement>& length_measurements,
    const std::vector<AngleMeasurement>& angle_measurements,
    const std::vector<RectangleAreaMeasurement>& rectangle_measurements,
    const std::vector<PolygonAreaMeasurement>& polygon_measurements,
    std::wstring& error,
    const CalibrationProfile* calibration)
{
    if (!frame.IsValid()) {
        error = L"No image frame to export.";
        return false;
    }

    const std::wstring extension = PathExtensionLower(path);
    if (extension.empty() || extension == L".bmp") {
        return SaveBmp(
            path,
            frame,
            length_measurements,
            angle_measurements,
            rectangle_measurements,
            polygon_measurements,
            error,
            calibration);
    }

    const GUID* container_format = WicContainerFormatForPath(path);
    if (!container_format) {
        error = L"Unsupported image export format.";
        return false;
    }

    ImageFrame output_frame = CopyForExport(frame);
    DrawMeasurements(
        output_frame,
        length_measurements,
        angle_measurements,
        rectangle_measurements,
        polygon_measurements);
    DrawScaleBar(output_frame, calibration);
    return SaveWicImage(path, output_frame, *container_format, error);
}

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
    std::wstring& error,
    const CalibrationProfile* calibration)
{
    return SaveBmp(path, frame, measurements, {}, {}, {}, error, calibration);
}

bool ImageExporter::SaveBmp(
    const std::filesystem::path& path,
    const ImageFrame& frame,
    const std::vector<LengthMeasurement>& length_measurements,
    const std::vector<AngleMeasurement>& angle_measurements,
    const std::vector<RectangleAreaMeasurement>& rectangle_measurements,
    const std::vector<PolygonAreaMeasurement>& polygon_measurements,
    std::wstring& error,
    const CalibrationProfile* calibration)
{
    if (!frame.IsValid()) {
        error = L"No image frame to export.";
        return false;
    }

    ImageFrame output_frame = CopyForExport(frame);
    DrawMeasurements(
        output_frame,
        length_measurements,
        angle_measurements,
        rectangle_measurements,
        polygon_measurements);
    DrawScaleBar(output_frame, calibration);

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
