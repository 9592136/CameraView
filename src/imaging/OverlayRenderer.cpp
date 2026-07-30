#include "OverlayRenderer.h"
#include "../ai/YoloEngine.h"

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

// ── AI Overlay rendering ──────────────────────────────────────────────────

void OverlayRenderer::DrawAiOverlay(
    HDC hdc,
    const RECT& viewport,
    const ImageFrame& frame,
    ImageViewport& image_viewport,
    const AiOverlayModel& model) const
{
    if (!model.show_detection_boxes && !model.show_segmentation_overlay) return;

    const RECT image_rect = image_viewport.ComputeImageRect(viewport, frame);
    if (image_rect.right <= image_rect.left || image_rect.bottom <= image_rect.top) return;

    const double scale_x = static_cast<double>(image_rect.right - image_rect.left) / frame.width;
    const double scale_y = static_cast<double>(image_rect.bottom - image_rect.top) / frame.height;

    const int saved_dc = SaveDC(hdc);
    IntersectClipRect(hdc, image_rect.left, image_rect.top, image_rect.right, image_rect.bottom);

    // Draw segmentation overlay first (behind boxes)
    if (model.show_segmentation_overlay && model.segmentation &&
        model.segmentation->width > 0 && model.segmentation->height > 0) {
        DrawSegmentationOverlay(hdc, viewport, frame, image_viewport, *model.segmentation, model.seg_alpha);
    }

    // Draw YOLO detection boxes (normalized coords) — takes priority
    if (model.show_detection_boxes && model.yolo_detections && !model.yolo_detections->empty()) {
        const std::vector<AiLabel> empty_labels;
        const auto& labels = model.labels ? *model.labels : empty_labels;
        DrawYoloDetectionBoxes(hdc, image_rect, frame, *model.yolo_detections, labels);
    }
    // Draw legacy detection boxes on top
    else if (model.show_detection_boxes && model.detections && !model.detections->empty()) {
        const std::vector<AiLabel> empty_labels;
        const auto& labels = model.labels ? *model.labels : empty_labels;
        DrawDetectionBoxes(hdc, viewport, image_viewport, *model.detections, labels);
    }

    // Draw classification label
    if (model.classification && model.classification->label_id >= 0) {
        DrawClassificationLabel(hdc, viewport, image_viewport, *model.classification);
    }

    if (saved_dc) {
        RestoreDC(hdc, saved_dc);
    }
}

void OverlayRenderer::DrawDetectionBoxes(
    HDC hdc,
    const RECT& viewport,
    ImageViewport& image_viewport,
    const std::vector<DetectionBox>& detections,
    const std::vector<AiLabel>& labels) const
{
    const RECT image_rect = image_viewport.ComputeImageRect(viewport, ImageFrame{});
    const int img_x = image_rect.left;
    const int img_y = image_rect.top;
    const int img_w = image_rect.right - image_rect.left;
    const int img_h = image_rect.bottom - image_rect.top;
    if (img_w <= 0 || img_h <= 0) return;

    // Need frame dimensions for scaling
    double scale_x = 1.0, scale_y = 1.0;
    int frame_w = 640, frame_h = 480;
    // Get frame info from first detection or use defaults
    if (!detections.empty()) {
        // We'll use the image_rect directly - assume frame fills it
        for (const auto& d : detections) {
            if (d.width > 0 && d.height > 0) {
                // estimate frame dimensions from max coords
                frame_w = std::max(frame_w, d.x + d.width + 10);
                frame_h = std::max(frame_h, d.y + d.height + 10);
            }
        }
        scale_x = static_cast<double>(img_w) / frame_w;
        scale_y = static_cast<double>(img_h) / frame_h;
    }

    SetBkMode(hdc, TRANSPARENT);

    for (const auto& det : detections) {
        int sx = img_x + static_cast<int>(det.x * scale_x);
        int sy = img_y + static_cast<int>(det.y * scale_y);
        int sw = static_cast<int>(det.width * scale_x);
        int sh = static_cast<int>(det.height * scale_y);

        // Clamp to viewport
        if (sx < image_rect.left) { sw -= (image_rect.left - sx); sx = image_rect.left; }
        if (sy < image_rect.top) { sh -= (image_rect.top - sy); sy = image_rect.top; }
        if (sx + sw > image_rect.right) sw = image_rect.right - sx;
        if (sy + sh > image_rect.bottom) sh = image_rect.bottom - sy;
        if (sw <= 0 || sh <= 0) continue;

        COLORREF color = GetLabelColorRef(det.label_id, labels);

        // Draw filled rect with alpha (semi-transparent fill)
        HPEN box_pen = CreatePen(PS_SOLID, 2, color);
        HBRUSH fill_brush = CreateSolidBrush(color);
        HGDIOBJ old_pen = SelectObject(hdc, box_pen);
        HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

        // Draw outer box
        Rectangle(hdc, sx, sy, sx + sw, sy + sh);

        // Draw semi-transparent fill
        RECT fill_rect{sx + 1, sy + 1, sx + sw, sy + sh};
        // Use a pattern brush for alpha effect
        HBRUSH alpha_brush = CreateSolidBrush(RGB(
            (GetRValue(color) + 255) / 2,
            (GetGValue(color) + 255) / 2,
            (GetBValue(color) + 255) / 2));
        FillRect(hdc, &fill_rect, alpha_brush);
        DeleteObject(alpha_brush);

        // Draw label background and text
        std::wstring label_text = det.label_name + L" " +
            std::to_wstring(static_cast<int>(det.confidence * 100)) + L"%";

        SIZE text_size;
        GetTextExtentPoint32W(hdc, label_text.c_str(), static_cast<int>(label_text.size()), &text_size);

        int label_x = sx;
        int label_y = sy - text_size.cy - 4;
        if (label_y < image_rect.top) label_y = sy + 2;

        RECT label_bg{label_x, label_y, label_x + text_size.cx + 6, label_y + text_size.cy + 4};

        // Draw label background
        HBRUSH label_bg_brush = CreateSolidBrush(color);
        FillRect(hdc, &label_bg, label_bg_brush);
        DeleteObject(label_bg_brush);

        SetTextColor(hdc, RGB(255, 255, 255));
        RECT label_text_rect = label_bg;
        label_text_rect.left += 3;
        DrawTextW(hdc, label_text.c_str(), -1, &label_text_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, old_pen);
        SelectObject(hdc, old_brush);
        DeleteObject(box_pen);
    }
}

void OverlayRenderer::DrawSegmentationOverlay(
    HDC hdc,
    const RECT& viewport,
    const ImageFrame& frame,
    ImageViewport& image_viewport,
    const SegmentationMask& mask,
    float alpha) const
{
    if (mask.width <= 0 || mask.height <= 0 || mask.data.empty()) return;

    const RECT image_rect = image_viewport.ComputeImageRect(viewport, frame);
    const int img_w = image_rect.right - image_rect.left;
    const int img_h = image_rect.bottom - image_rect.top;
    if (img_w <= 0 || img_h <= 0) return;

    const double scale_x = static_cast<double>(img_w) / mask.width;
    const double scale_y = static_cast<double>(img_h) / mask.height;

    // Create a DIB section for the overlay bitmap (32-bit BGRA)
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = img_w;
    bmi.bmiHeader.biHeight = -img_h; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    uint8_t* bits = nullptr;
    HBITMAP hBmp = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS,
        reinterpret_cast<void**>(&bits), nullptr, 0);
    if (!hBmp || !bits) return;

    // Predefined colors for different segments
    static const COLORREF segment_colors[] = {
        RGB(255, 100, 100),  // red
        RGB(100, 255, 100),  // green
        RGB(100, 100, 255),  // blue
        RGB(255, 255, 100),  // yellow
        RGB(255, 100, 255),  // magenta
        RGB(100, 255, 255),  // cyan
        RGB(255, 180, 80),   // orange
        RGB(180, 100, 255),  // purple
    };
    constexpr int num_colors = sizeof(segment_colors) / sizeof(segment_colors[0]);

    uint8_t alpha_byte = static_cast<uint8_t>(std::max(0.0f, std::min(1.0f, alpha)) * 255.0f);

    for (int y = 0; y < img_h; ++y) {
        for (int x = 0; x < img_w; ++x) {
            int mask_x = static_cast<int>(x / scale_x);
            int mask_y = static_cast<int>(y / scale_y);
            mask_x = std::max(0, std::min(mask.width - 1, mask_x));
            mask_y = std::max(0, std::min(mask.height - 1, mask_y));

            uint8_t seg_id = mask.data[static_cast<size_t>(mask_y) * mask.width + mask_x];
            size_t pixel_offset = (static_cast<size_t>(y) * img_w + x) * 4;

            if (seg_id == 0) {
                bits[pixel_offset + 0] = 0;  // B
                bits[pixel_offset + 1] = 0;  // G
                bits[pixel_offset + 2] = 0;  // R
                bits[pixel_offset + 3] = 0;  // A (transparent)
            } else {
                COLORREF c = segment_colors[(seg_id - 1) % num_colors];
                // Alpha blend with white background
                bits[pixel_offset + 0] = static_cast<uint8_t>((GetBValue(c) * alpha_byte + 255 * (255 - alpha_byte)) / 255);
                bits[pixel_offset + 1] = static_cast<uint8_t>((GetGValue(c) * alpha_byte + 255 * (255 - alpha_byte)) / 255);
                bits[pixel_offset + 2] = static_cast<uint8_t>((GetRValue(c) * alpha_byte + 255 * (255 - alpha_byte)) / 255);
                bits[pixel_offset + 3] = alpha_byte;
            }
        }
    }

    // Alpha-blend the overlay bitmap onto the DC
    HDC mem_dc = CreateCompatibleDC(hdc);
    HBITMAP old_bmp = static_cast<HBITMAP>(SelectObject(mem_dc, hBmp));

    BLENDFUNCTION bf = {};
    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = alpha_byte;
    bf.AlphaFormat = AC_SRC_ALPHA;

    AlphaBlend(hdc,
        image_rect.left, image_rect.top, img_w, img_h,
        mem_dc,
        0, 0, img_w, img_h,
        bf);

    SelectObject(mem_dc, old_bmp);
    DeleteDC(mem_dc);
    DeleteObject(hBmp);
}

void OverlayRenderer::DrawClassificationLabel(
    HDC hdc,
    const RECT& viewport,
    ImageViewport& image_viewport,
    const ClassificationResult& cls) const
{
    if (cls.label_id < 0) return;

    const RECT image_rect = image_viewport.ComputeImageRect(viewport, ImageFrame{});
    if (image_rect.right <= image_rect.left || image_rect.bottom <= image_rect.top) return;

    SetBkMode(hdc, TRANSPARENT);

    // Large centered label at top of image
    wchar_t buf[256];
    swprintf(buf, 256, L"%s (%.1f%%)", cls.label_name.c_str(), cls.confidence * 100.0f);

    SIZE text_size;
    HFONT big_font = CreateFontW(
        28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Arial");
    HGDIOBJ old_font = SelectObject(hdc, big_font);

    GetTextExtentPoint32W(hdc, buf, static_cast<int>(wcslen(buf)), &text_size);

    int label_x = image_rect.left + (image_rect.right - image_rect.left - text_size.cx) / 2;
    int label_y = image_rect.top + 16;

    // Draw shadow
    SetTextColor(hdc, RGB(0, 0, 0));
    RECT shadow_rect{label_x + 1, label_y + 1, label_x + text_size.cx + 1, label_y + text_size.cy + 1};
    DrawTextW(hdc, buf, -1, &shadow_rect, DT_LEFT | DT_TOP);

    // Draw text with bright color
    SetTextColor(hdc, RGB(100, 255, 100));
    RECT text_rect{label_x, label_y, label_x + text_size.cx, label_y + text_size.cy};
    DrawTextW(hdc, buf, -1, &text_rect, DT_LEFT | DT_TOP);

    SelectObject(hdc, old_font);
    DeleteObject(big_font);
}

// ── YOLO detection box rendering (normalized coordinates) ─────────────────

void OverlayRenderer::DrawYoloDetectionBoxes(
    HDC hdc,
    const RECT& image_rect,
    const ImageFrame& frame,
    const std::vector<YoloDetection>& detections,
    const std::vector<AiLabel>& labels) const
{
    if (detections.empty()) return;

    int img_x = image_rect.left;
    int img_y = image_rect.top;
    int img_w = image_rect.right - image_rect.left;
    int img_h = image_rect.bottom - image_rect.top;

    for (const auto& det : detections) {
        // Convert normalized [0,1] coords to screen pixels
        int bx = img_x + static_cast<int>(det.x * img_w);
        int by = img_y + static_cast<int>(det.y * img_h);
        int bw = static_cast<int>(det.w * img_w);
        int bh = static_cast<int>(det.h * img_h);

        if (bw < 4) bw = 4;
        if (bh < 4) bh = 4;

        COLORREF color = GetLabelColorRef(det.class_id, labels);

        // Semi-transparent fill
        HPEN pen = CreatePen(PS_SOLID, 3, color);
        HGDIOBJ old_pen = SelectObject(hdc, pen);
        HBRUSH fill_brush = CreateSolidBrush(
            RGB(GetRValue(color) / 3, GetGValue(color) / 3, GetBValue(color) / 3));
        HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

        Rectangle(hdc, bx, by, bx + bw, by + bh);

        // YOLO-style label background + text
        wchar_t label_buf[128];
        swprintf(label_buf, 128, L"%s %.1f%%",
            det.class_name.c_str(), det.confidence * 100.0f);

        SIZE text_size;
        HFONT label_font = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Arial");
        HGDIOBJ old_font = SelectObject(hdc, label_font);
        GetTextExtentPoint32W(hdc, label_buf, static_cast<int>(wcslen(label_buf)), &text_size);

        // Label background
        RECT label_rect{bx, by - text_size.cy - 4, bx + text_size.cx + 8, by};
        HBRUSH label_bg = CreateSolidBrush(color);
        FillRect(hdc, &label_rect, label_bg);
        DeleteObject(label_bg);

        // Label text
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        DrawTextW(hdc, label_buf, -1, &label_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, old_font);
        DeleteObject(label_font);
        SelectObject(hdc, old_pen);
        SelectObject(hdc, old_brush);
        DeleteObject(pen);
    }
}

COLORREF OverlayRenderer::GetLabelColorRef(int label_id, const std::vector<AiLabel>& labels)
{
    static const COLORREF default_colors[] = {
        RGB(255, 80, 80),
        RGB(80, 255, 80),
        RGB(80, 80, 255),
        RGB(255, 255, 80),
        RGB(255, 80, 255),
        RGB(80, 255, 255),
        RGB(255, 160, 60),
        RGB(160, 80, 255),
    };
    constexpr int num_colors = sizeof(default_colors) / sizeof(default_colors[0]);

    for (const auto& label : labels) {
        if (label.id == label_id) {
            return label.color;
        }
    }
    return default_colors[label_id % num_colors];
}
