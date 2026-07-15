#include "OverlayRenderer.h"

#include "../domain/MeasurementFormatter.h"

namespace {

bool HasPendingPoint(OverlayPendingKind kind)
{
    return kind == OverlayPendingKind::Point ||
           kind == OverlayPendingKind::Angle ||
           kind == OverlayPendingKind::Polygon;
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
