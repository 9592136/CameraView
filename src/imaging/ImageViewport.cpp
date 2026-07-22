#include "ImageViewport.h"

#include <cstddef>

namespace {

bool HasReadableBgrRows(const ImageFrame& frame)
{
    if (!frame.IsValid() || frame.stride < frame.width * 3) {
        return false;
    }

    const std::size_t required =
        static_cast<std::size_t>(frame.height - 1) * static_cast<std::size_t>(frame.stride) +
        static_cast<std::size_t>(frame.width) * 3U;
    return frame.bgr.size() >= required;
}

} // namespace

void ImageViewport::Reset()
{
    transform_.Reset();
}

bool ImageViewport::ZoomAt(const RECT& viewport, const ImageFrame& frame, POINT point, short wheel_delta)
{
    if (!frame.IsValid()) {
        return false;
    }
    return transform_.ZoomAt(viewport, frame.width, frame.height, point, wheel_delta);
}

bool ImageViewport::PanBy(const RECT& viewport, const ImageFrame& frame, int delta_x, int delta_y)
{
    if (!frame.IsValid()) {
        return false;
    }
    return transform_.PanBy(viewport, frame.width, frame.height, delta_x, delta_y);
}

std::optional<ImagePoint> ImageViewport::ScreenToImage(const RECT& viewport, const ImageFrame& frame, POINT point)
{
    if (!frame.IsValid()) {
        return std::nullopt;
    }
    return transform_.ScreenToImage(viewport, frame.width, frame.height, point);
}

POINT ImageViewport::ImageToScreen(const RECT& viewport, const ImageFrame& frame, ImagePoint point)
{
    if (!frame.IsValid()) {
        return POINT{viewport.left, viewport.top};
    }
    return transform_.ImageToScreen(viewport, frame.width, frame.height, point);
}

RECT ImageViewport::ComputeImageRect(const RECT& viewport, const ImageFrame& frame)
{
    if (!frame.IsValid()) {
        return viewport;
    }
    return transform_.ComputeImageRect(viewport, frame.width, frame.height);
}

void ImageViewport::DrawFrame(HDC hdc, const RECT& viewport, const ImageFrame& frame)
{
    const int area_width = viewport.right - viewport.left;
    const int area_height = viewport.bottom - viewport.top;
    if (area_width <= 0 || area_height <= 0 || !HasReadableBgrRows(frame)) {
        return;
    }

    const RECT image_rect = ComputeImageRect(viewport, frame);
    const int draw_left = image_rect.left;
    const int draw_top = image_rect.top;
    const int draw_width = image_rect.right - image_rect.left;
    const int draw_height = image_rect.bottom - image_rect.top;

    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = frame.width;
    info.bmiHeader.biHeight = -frame.height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 24;
    info.bmiHeader.biCompression = BI_RGB;

    const int saved_dc = SaveDC(hdc);
    IntersectClipRect(hdc, viewport.left, viewport.top, viewport.right, viewport.bottom);
    SetStretchBltMode(hdc, COLORONCOLOR);
    StretchDIBits(
        hdc,
        draw_left,
        draw_top,
        draw_width,
        draw_height,
        0,
        0,
        frame.width,
        frame.height,
        frame.bgr.data(),
        &info,
        DIB_RGB_COLORS,
        SRCCOPY);
    if (saved_dc) {
        RestoreDC(hdc, saved_dc);
    }
}
