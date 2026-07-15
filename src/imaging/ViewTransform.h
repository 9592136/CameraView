#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "../domain/Geometry.h"

#include <optional>

class ViewTransform {
public:
    double Zoom() const { return zoom_factor_; }

    void Reset();
    bool ZoomAt(const RECT& viewport, int image_width, int image_height, POINT point, short wheel_delta);
    bool PanBy(const RECT& viewport, int image_width, int image_height, int delta_x, int delta_y);
    std::optional<ImagePoint> ScreenToImage(const RECT& viewport, int image_width, int image_height, POINT point);
    POINT ImageToScreen(const RECT& viewport, int image_width, int image_height, ImagePoint point);
    RECT ComputeImageRect(const RECT& viewport, int image_width, int image_height);

private:
    void ClampCenter(const RECT& viewport, int image_width, int image_height, double zoom);

    double zoom_factor_ = 1.0;
    double center_x_ = 0.5;
    double center_y_ = 0.5;
};
