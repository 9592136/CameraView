#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "../domain/Geometry.h"
#include "../domain/ImageFrame.h"
#include "ViewTransform.h"

#include <optional>

class ImageViewport {
public:
    double Zoom() const { return transform_.Zoom(); }

    void Reset();
    bool ZoomAt(const RECT& viewport, const ImageFrame& frame, POINT point, short wheel_delta);
    bool PanBy(const RECT& viewport, const ImageFrame& frame, int delta_x, int delta_y);
    std::optional<ImagePoint> ScreenToImage(const RECT& viewport, const ImageFrame& frame, POINT point);
    POINT ImageToScreen(const RECT& viewport, const ImageFrame& frame, ImagePoint point);
    RECT ComputeImageRect(const RECT& viewport, const ImageFrame& frame);
    void DrawFrame(HDC hdc, const RECT& viewport, const ImageFrame& frame);

private:
    ViewTransform transform_;
};
