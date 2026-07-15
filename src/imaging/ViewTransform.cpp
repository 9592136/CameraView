#include "ViewTransform.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kMinZoom = 0.25;
constexpr double kMaxZoom = 8.0;
constexpr double kZoomStep = 1.1;

bool HasDrawableArea(const RECT& viewport, int image_width, int image_height)
{
    return viewport.right > viewport.left &&
           viewport.bottom > viewport.top &&
           image_width > 0 &&
           image_height > 0;
}

double ComputeBaseScale(const RECT& viewport, int image_width, int image_height)
{
    const int area_width = viewport.right - viewport.left;
    const int area_height = viewport.bottom - viewport.top;
    return std::min(
        static_cast<double>(area_width) / static_cast<double>(image_width),
        static_cast<double>(area_height) / static_cast<double>(image_height));
}

} // namespace

void ViewTransform::Reset()
{
    zoom_factor_ = 1.0;
    center_x_ = 0.5;
    center_y_ = 0.5;
}

bool ViewTransform::ZoomAt(const RECT& viewport, int image_width, int image_height, POINT point, short wheel_delta)
{
    if (wheel_delta == 0) {
        return false;
    }

    const double old_zoom = zoom_factor_;
    const double factor = std::pow(kZoomStep, static_cast<double>(wheel_delta) / WHEEL_DELTA);
    const double new_zoom = std::clamp(old_zoom * factor, kMinZoom, kMaxZoom);
    if (new_zoom == old_zoom) {
        return true;
    }

    if (!HasDrawableArea(viewport, image_width, image_height)) {
        Reset();
        zoom_factor_ = new_zoom;
        return true;
    }

    ClampCenter(viewport, image_width, image_height, old_zoom);

    const double base_scale = ComputeBaseScale(viewport, image_width, image_height);
    const double old_draw_width = image_width * base_scale * old_zoom;
    const double old_draw_height = image_height * base_scale * old_zoom;
    const double viewport_center_x = (viewport.left + viewport.right) / 2.0;
    const double viewport_center_y = (viewport.top + viewport.bottom) / 2.0;
    const double old_left = viewport_center_x - center_x_ * old_draw_width;
    const double old_top = viewport_center_y - center_y_ * old_draw_height;
    const double image_x = std::clamp((point.x - old_left) / old_draw_width, 0.0, 1.0);
    const double image_y = std::clamp((point.y - old_top) / old_draw_height, 0.0, 1.0);
    const double new_draw_width = image_width * base_scale * new_zoom;
    const double new_draw_height = image_height * base_scale * new_zoom;

    center_x_ = image_x - (point.x - viewport_center_x) / new_draw_width;
    center_y_ = image_y - (point.y - viewport_center_y) / new_draw_height;
    zoom_factor_ = new_zoom;
    ClampCenter(viewport, image_width, image_height, zoom_factor_);
    return true;
}

bool ViewTransform::PanBy(const RECT& viewport, int image_width, int image_height, int delta_x, int delta_y)
{
    if ((delta_x == 0 && delta_y == 0) || !HasDrawableArea(viewport, image_width, image_height)) {
        return false;
    }

    const double base_scale = ComputeBaseScale(viewport, image_width, image_height);
    const double draw_width = image_width * base_scale * zoom_factor_;
    const double draw_height = image_height * base_scale * zoom_factor_;
    if (draw_width <= 0.0 || draw_height <= 0.0) {
        return false;
    }

    const double old_center_x = center_x_;
    const double old_center_y = center_y_;
    center_x_ -= static_cast<double>(delta_x) / draw_width;
    center_y_ -= static_cast<double>(delta_y) / draw_height;
    ClampCenter(viewport, image_width, image_height, zoom_factor_);
    return center_x_ != old_center_x || center_y_ != old_center_y;
}

std::optional<ImagePoint> ViewTransform::ScreenToImage(const RECT& viewport, int image_width, int image_height, POINT point)
{
    if (!HasDrawableArea(viewport, image_width, image_height)) {
        return std::nullopt;
    }

    const RECT image_rect = ComputeImageRect(viewport, image_width, image_height);
    const int draw_width = image_rect.right - image_rect.left;
    const int draw_height = image_rect.bottom - image_rect.top;
    if (draw_width <= 0 || draw_height <= 0) {
        return std::nullopt;
    }
    if (point.x < image_rect.left ||
        point.x > image_rect.right ||
        point.y < image_rect.top ||
        point.y > image_rect.bottom) {
        return std::nullopt;
    }

    ImagePoint image_point;
    image_point.x = std::clamp(
        (static_cast<double>(point.x) - image_rect.left) / draw_width * image_width,
        0.0,
        static_cast<double>(image_width));
    image_point.y = std::clamp(
        (static_cast<double>(point.y) - image_rect.top) / draw_height * image_height,
        0.0,
        static_cast<double>(image_height));
    return image_point;
}

POINT ViewTransform::ImageToScreen(const RECT& viewport, int image_width, int image_height, ImagePoint point)
{
    POINT output = {viewport.left, viewport.top};
    if (!HasDrawableArea(viewport, image_width, image_height)) {
        return output;
    }

    const RECT image_rect = ComputeImageRect(viewport, image_width, image_height);
    const int draw_width = image_rect.right - image_rect.left;
    const int draw_height = image_rect.bottom - image_rect.top;
    if (draw_width <= 0 || draw_height <= 0) {
        return output;
    }

    output.x = static_cast<int>(std::round(image_rect.left + point.x / image_width * draw_width));
    output.y = static_cast<int>(std::round(image_rect.top + point.y / image_height * draw_height));
    return output;
}

RECT ViewTransform::ComputeImageRect(const RECT& viewport, int image_width, int image_height)
{
    RECT output = viewport;
    if (!HasDrawableArea(viewport, image_width, image_height)) {
        return output;
    }

    ClampCenter(viewport, image_width, image_height, zoom_factor_);

    const double scale = ComputeBaseScale(viewport, image_width, image_height) * zoom_factor_;
    const int draw_width = std::max(1, static_cast<int>(std::round(image_width * scale)));
    const int draw_height = std::max(1, static_cast<int>(std::round(image_height * scale)));
    const double viewport_center_x = (viewport.left + viewport.right) / 2.0;
    const double viewport_center_y = (viewport.top + viewport.bottom) / 2.0;
    const int draw_left = static_cast<int>(std::round(viewport_center_x - center_x_ * draw_width));
    const int draw_top = static_cast<int>(std::round(viewport_center_y - center_y_ * draw_height));

    output.left = draw_left;
    output.top = draw_top;
    output.right = draw_left + draw_width;
    output.bottom = draw_top + draw_height;
    return output;
}

void ViewTransform::ClampCenter(const RECT& viewport, int image_width, int image_height, double zoom)
{
    if (!HasDrawableArea(viewport, image_width, image_height)) {
        center_x_ = 0.5;
        center_y_ = 0.5;
        return;
    }

    const int area_width = viewport.right - viewport.left;
    const int area_height = viewport.bottom - viewport.top;
    const double base_scale = ComputeBaseScale(viewport, image_width, image_height);
    const double draw_width = image_width * base_scale * zoom;
    const double draw_height = image_height * base_scale * zoom;

    if (draw_width <= area_width) {
        center_x_ = 0.5;
    } else {
        const double margin = area_width / (2.0 * draw_width);
        center_x_ = std::clamp(center_x_, margin, 1.0 - margin);
    }

    if (draw_height <= area_height) {
        center_y_ = 0.5;
    } else {
        const double margin = area_height / (2.0 * draw_height);
        center_y_ = std::clamp(center_y_, margin, 1.0 - margin);
    }
}
