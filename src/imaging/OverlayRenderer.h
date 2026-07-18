#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "../domain/CalibrationProfile.h"
#include "../domain/Geometry.h"
#include "../domain/ImageFrame.h"
#include "../domain/Measurement.h"
#include "ImageViewport.h"

#include <string>
#include <vector>

enum class OverlayPendingKind {
    None,
    Point,
    Angle,
    Polygon
};

struct MeasurementOverlayPending {
    OverlayPendingKind kind = OverlayPendingKind::None;
    ImagePoint first;
    ImagePoint second;
    const std::vector<ImagePoint>* polygon_points = nullptr;
};

struct MeasurementOverlayModel {
    const CalibrationProfile* calibration = nullptr;
    MeasurementUnit display_unit = MeasurementUnit::Pixels;
    const std::vector<LengthMeasurement>* lengths = nullptr;
    const std::vector<AngleMeasurement>* angles = nullptr;
    const std::vector<RectangleAreaMeasurement>* rectangles = nullptr;
    const std::vector<PolygonAreaMeasurement>* polygons = nullptr;
    MeasurementOverlayPending pending;
};

struct ScaleBarOverlay {
    bool visible = false;
    int screen_length = 0;
    std::wstring label;
};

class OverlayRenderer {
public:
    void DrawMeasurementOverlay(
        HDC hdc,
        const RECT& viewport,
        const ImageFrame& frame,
        ImageViewport& image_viewport,
        const MeasurementOverlayModel& model) const;

    static std::wstring FormatLine(
        const LengthMeasurement& measurement,
        const CalibrationProfile& calibration,
        MeasurementUnit display_unit);
    static std::wstring FormatLine(const AngleMeasurement& measurement);
    static std::wstring FormatLine(
        const RectangleAreaMeasurement& measurement,
        const CalibrationProfile& calibration,
        MeasurementUnit display_unit);
    static std::wstring FormatLine(
        const PolygonAreaMeasurement& measurement,
        const CalibrationProfile& calibration,
        MeasurementUnit display_unit);

    static ScaleBarOverlay BuildScaleBarOverlay(
        const CalibrationProfile& calibration,
        int viewport_width,
        double screen_pixels_per_image_pixel);

private:
    static void DrawPointHandle(HDC hdc, POINT point, int radius);
    static void DrawScaleBar(
        HDC hdc,
        const RECT& viewport,
        const ImageFrame& frame,
        ImageViewport& image_viewport,
        const CalibrationProfile& calibration);
};
