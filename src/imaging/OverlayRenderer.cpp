#include "OverlayRenderer.h"

#include "../domain/MeasurementFormatter.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace {

bool HasPendingPoint(OverlayPendingKind kind)
{
    return kind == OverlayPendingKind::Point ||
           kind == OverlayPendingKind::Angle ||
           kind == OverlayPendingKind::Polygon;
}

double NiceLengthAtOrBelow(double target)
{
    if (!std::isfinite(target) || target <= 0.0) {
        return 0.0;
    }

    const double base = std::pow(10.0, std::floor(std::log10(target)));
    double best = base;
    for (double multiplier : {1.0, 2.0, 5.0, 10.0}) {
        const double candidate = base * multiplier;
        if (candidate <= target * 1.0000001) {
            best = candidate;
        }
    }
    return best;
}

std::wstring FormatScaleValue(double value)
{
    std::wostringstream output;
    if (std::abs(value - std::round(value)) < 1e-9 || value >= 100.0) {
        output << std::fixed << std::setprecision(0) << value;
    } else if (value >= 10.0) {
        output << std::fixed << std::setprecision(1) << value;
    } else {
        output << std::fixed << std::setprecision(2) << value;
    }

    std::wstring text = output.str();
    if (text.find(L'.') != std::wstring::npos) {
        while (text.size() > 1U && text.back() == L'0') {
            text.pop_back();
        }
        if (!text.empty() && text.back() == L'.') {
            text.pop_back();
        }
    }
    return text;
}

std::wstring FormatScaleLabel(double micrometers)
{
    if (micrometers >= 1000.0) {
        return FormatScaleValue(micrometers / 1000.0) + L" " +
            CalibrationProfile::UnitLabel(MeasurementUnit::Millimeters);
    }
    return FormatScaleValue(micrometers) + L" " +
        CalibrationProfile::UnitLabel(MeasurementUnit::Micrometers);
}

} // namespace

void OverlayRenderer::DrawMeasurementOverlay(
    HDC hdc,
    const RECT& viewport,
    const ImageFrame& frame,
    ImageViewport& image_viewport,
    const MeasurementOverlayModel& model) const
{
    if (!frame.IsValid()) {
        return;
    }

    const CalibrationProfile fallback_calibration = CalibrationProfile::Uncalibrated();
    const CalibrationProfile& calibration = model.calibration ? *model.calibration : fallback_calibration;

    const int saved_dc = SaveDC(hdc);
    IntersectClipRect(hdc, viewport.left, viewport.top, viewport.right, viewport.bottom);
    SetBkMode(hdc, TRANSPARENT);

    HPEN measurement_pen = CreatePen(PS_SOLID, 2, RGB(255, 214, 74));
    HBRUSH measurement_brush = CreateSolidBrush(RGB(255, 214, 74));
    HGDIOBJ old_pen = SelectObject(hdc, measurement_pen);
    HGDIOBJ old_brush = SelectObject(hdc, measurement_brush);
    SetTextColor(hdc, RGB(255, 234, 158));

    if (model.lengths) {
        for (const LengthMeasurement& measurement : *model.lengths) {
            const POINT first = image_viewport.ImageToScreen(viewport, frame, measurement.First());
            const POINT second = image_viewport.ImageToScreen(viewport, frame, measurement.Second());
            MoveToEx(hdc, first.x, first.y, nullptr);
            LineTo(hdc, second.x, second.y);
            DrawPointHandle(hdc, first, 4);
            DrawPointHandle(hdc, second, 4);

            RECT label = {
                (first.x + second.x) / 2 + 6,
                (first.y + second.y) / 2 - 18,
                (first.x + second.x) / 2 + 220,
                (first.y + second.y) / 2 + 18
            };
            const std::wstring text = FormatLine(measurement, calibration, model.display_unit);
            DrawTextW(hdc, text.c_str(), -1, &label, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
    }

    if (model.angles) {
        for (const AngleMeasurement& measurement : *model.angles) {
            const POINT first = image_viewport.ImageToScreen(viewport, frame, measurement.First());
            const POINT vertex = image_viewport.ImageToScreen(viewport, frame, measurement.Vertex());
            const POINT second = image_viewport.ImageToScreen(viewport, frame, measurement.Second());
            MoveToEx(hdc, vertex.x, vertex.y, nullptr);
            LineTo(hdc, first.x, first.y);
            MoveToEx(hdc, vertex.x, vertex.y, nullptr);
            LineTo(hdc, second.x, second.y);
            DrawPointHandle(hdc, first, 4);
            DrawPointHandle(hdc, vertex, 4);
            DrawPointHandle(hdc, second, 4);

            RECT label = {
                vertex.x + 8,
                vertex.y - 18,
                vertex.x + 220,
                vertex.y + 18
            };
            const std::wstring text = FormatLine(measurement);
            DrawTextW(hdc, text.c_str(), -1, &label, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
    }

    if (model.rectangles) {
        for (const RectangleAreaMeasurement& measurement : *model.rectangles) {
            const POINT first = image_viewport.ImageToScreen(viewport, frame, measurement.First());
            const POINT second = image_viewport.ImageToScreen(viewport, frame, measurement.Second());
            MoveToEx(hdc, first.x, first.y, nullptr);
            LineTo(hdc, second.x, first.y);
            LineTo(hdc, second.x, second.y);
            LineTo(hdc, first.x, second.y);
            LineTo(hdc, first.x, first.y);
            DrawPointHandle(hdc, first, 4);
            DrawPointHandle(hdc, second, 4);

            RECT label = {
                (first.x + second.x) / 2 + 6,
                (first.y + second.y) / 2 - 18,
                (first.x + second.x) / 2 + 220,
                (first.y + second.y) / 2 + 18
            };
            const std::wstring text = FormatLine(measurement, calibration, model.display_unit);
            DrawTextW(hdc, text.c_str(), -1, &label, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
    }

    if (model.polygons) {
        for (const PolygonAreaMeasurement& measurement : *model.polygons) {
            const std::vector<ImagePoint>& points = measurement.Points();
            if (points.empty()) {
                continue;
            }

            int sum_x = 0;
            int sum_y = 0;
            for (std::size_t index = 0; index < points.size(); ++index) {
                const POINT current = image_viewport.ImageToScreen(viewport, frame, points[index]);
                const POINT next = image_viewport.ImageToScreen(viewport, frame, points[(index + 1) % points.size()]);
                MoveToEx(hdc, current.x, current.y, nullptr);
                LineTo(hdc, next.x, next.y);
                DrawPointHandle(hdc, current, 4);
                sum_x += current.x;
                sum_y += current.y;
            }

            RECT label = {
                sum_x / static_cast<int>(points.size()) + 6,
                sum_y / static_cast<int>(points.size()) - 18,
                sum_x / static_cast<int>(points.size()) + 220,
                sum_y / static_cast<int>(points.size()) + 18
            };
            const std::wstring text = FormatLine(measurement, calibration, model.display_unit);
            DrawTextW(hdc, text.c_str(), -1, &label, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
    }

    if (HasPendingPoint(model.pending.kind)) {
        HPEN pending_pen = CreatePen(PS_SOLID, 2, RGB(111, 211, 255));
        HBRUSH pending_brush = CreateSolidBrush(RGB(111, 211, 255));
        SelectObject(hdc, pending_pen);
        SelectObject(hdc, pending_brush);

        if (model.pending.kind == OverlayPendingKind::Polygon && model.pending.polygon_points) {
            for (std::size_t index = 0; index < model.pending.polygon_points->size(); ++index) {
                const POINT current = image_viewport.ImageToScreen(viewport, frame, (*model.pending.polygon_points)[index]);
                DrawPointHandle(hdc, current, 5);
                if (index > 0) {
                    const POINT previous = image_viewport.ImageToScreen(viewport, frame, (*model.pending.polygon_points)[index - 1]);
                    MoveToEx(hdc, previous.x, previous.y, nullptr);
                    LineTo(hdc, current.x, current.y);
                }
            }
        } else {
            const POINT pending = image_viewport.ImageToScreen(viewport, frame, model.pending.first);
            DrawPointHandle(hdc, pending, 5);
            if (model.pending.kind == OverlayPendingKind::Angle) {
                const POINT vertex = image_viewport.ImageToScreen(viewport, frame, model.pending.second);
                MoveToEx(hdc, vertex.x, vertex.y, nullptr);
                LineTo(hdc, pending.x, pending.y);
                DrawPointHandle(hdc, vertex, 5);
            }
        }

        SelectObject(hdc, measurement_pen);
        SelectObject(hdc, measurement_brush);
        DeleteObject(pending_pen);
        DeleteObject(pending_brush);
    }

    DrawScaleBar(hdc, viewport, frame, image_viewport, calibration);

    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(measurement_brush);
    DeleteObject(measurement_pen);
    if (saved_dc) {
        RestoreDC(hdc, saved_dc);
    }
}

std::wstring OverlayRenderer::FormatLine(
    const LengthMeasurement& measurement,
    const CalibrationProfile& calibration,
    MeasurementUnit display_unit)
{
    return MeasurementFormatter::FormatLine(measurement, calibration, display_unit);
}

ScaleBarOverlay OverlayRenderer::BuildScaleBarOverlay(
    const CalibrationProfile& calibration,
    int viewport_width,
    double screen_pixels_per_image_pixel)
{
    ScaleBarOverlay overlay;
    if (!calibration.IsCalibrated() ||
        viewport_width < 80 ||
        !std::isfinite(screen_pixels_per_image_pixel) ||
        screen_pixels_per_image_pixel <= 0.0) {
        return overlay;
    }

    int target_screen_length = std::clamp(viewport_width / 5, 80, 160);
    target_screen_length = std::min(target_screen_length, viewport_width - 48);
    if (target_screen_length < 40) {
        return overlay;
    }

    const double target_image_pixels =
        static_cast<double>(target_screen_length) / screen_pixels_per_image_pixel;
    const double target_micrometers = target_image_pixels * calibration.MicronsPerPixel();
    const double scale_micrometers = NiceLengthAtOrBelow(target_micrometers);
    if (scale_micrometers <= 0.0) {
        return overlay;
    }

    const int screen_length = static_cast<int>(std::round(
        scale_micrometers / calibration.MicronsPerPixel() *
        screen_pixels_per_image_pixel));
    if (screen_length < 24 || screen_length > viewport_width - 48) {
        return overlay;
    }

    overlay.visible = true;
    overlay.screen_length = screen_length;
    overlay.label = FormatScaleLabel(scale_micrometers);
    return overlay;
}

std::wstring OverlayRenderer::FormatLine(const AngleMeasurement& measurement)
{
    return MeasurementFormatter::FormatLine(measurement);
}

std::wstring OverlayRenderer::FormatLine(
    const RectangleAreaMeasurement& measurement,
    const CalibrationProfile& calibration,
    MeasurementUnit display_unit)
{
    return MeasurementFormatter::FormatLine(measurement, calibration, display_unit);
}

std::wstring OverlayRenderer::FormatLine(
    const PolygonAreaMeasurement& measurement,
    const CalibrationProfile& calibration,
    MeasurementUnit display_unit)
{
    return MeasurementFormatter::FormatLine(measurement, calibration, display_unit);
}

void OverlayRenderer::DrawPointHandle(HDC hdc, POINT point, int radius)
{
    Ellipse(hdc, point.x - radius, point.y - radius, point.x + radius + 1, point.y + radius + 1);
}

void OverlayRenderer::DrawScaleBar(
    HDC hdc,
    const RECT& viewport,
    const ImageFrame& frame,
    ImageViewport& image_viewport,
    const CalibrationProfile& calibration)
{
    const RECT image_rect = image_viewport.ComputeImageRect(viewport, frame);
    const int image_screen_width = image_rect.right - image_rect.left;
    if (image_screen_width <= 0 || frame.width <= 0) {
        return;
    }

    const ScaleBarOverlay overlay = BuildScaleBarOverlay(
        calibration,
        viewport.right - viewport.left,
        static_cast<double>(image_screen_width) / static_cast<double>(frame.width));
    if (!overlay.visible) {
        return;
    }

    const int saved_dc = SaveDC(hdc);
    IntersectClipRect(hdc, viewport.left, viewport.top, viewport.right, viewport.bottom);
    SetBkMode(hdc, TRANSPARENT);

    const int margin = 24;
    const int x1 = viewport.right - margin;
    const int x0 = x1 - overlay.screen_length;
    const int y = viewport.bottom - margin;
    const int tick = 7;

    HPEN shadow_pen = CreatePen(PS_SOLID, 6, RGB(0, 0, 0));
    HPEN bar_pen = CreatePen(PS_SOLID, 3, RGB(255, 255, 255));
    HGDIOBJ old_pen = SelectObject(hdc, shadow_pen);
    MoveToEx(hdc, x0, y, nullptr);
    LineTo(hdc, x1, y);
    MoveToEx(hdc, x0, y - tick, nullptr);
    LineTo(hdc, x0, y + tick);
    MoveToEx(hdc, x1, y - tick, nullptr);
    LineTo(hdc, x1, y + tick);

    SelectObject(hdc, bar_pen);
    MoveToEx(hdc, x0, y, nullptr);
    LineTo(hdc, x1, y);
    MoveToEx(hdc, x0, y - tick, nullptr);
    LineTo(hdc, x0, y + tick);
    MoveToEx(hdc, x1, y - tick, nullptr);
    LineTo(hdc, x1, y + tick);

    RECT label_rect{x0 - 40, y - 28, x1 + 2, y - 8};
    SetTextColor(hdc, RGB(0, 0, 0));
    RECT shadow_label = label_rect;
    OffsetRect(&shadow_label, 1, 1);
    DrawTextW(hdc, overlay.label.c_str(), -1, &shadow_label, DT_RIGHT | DT_SINGLELINE | DT_END_ELLIPSIS);
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawTextW(hdc, overlay.label.c_str(), -1, &label_rect, DT_RIGHT | DT_SINGLELINE | DT_END_ELLIPSIS);

    SelectObject(hdc, old_pen);
    DeleteObject(bar_pen);
    DeleteObject(shadow_pen);
    if (saved_dc) {
        RestoreDC(hdc, saved_dc);
    }
}
