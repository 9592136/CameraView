#pragma once

#include <windows.h>

#include "../domain/CalibrationProfile.h"
#include "../domain/Geometry.h"
#include "../domain/ImageFrame.h"
#include "../domain/Measurement.h"
#include "../ai/ModelDef.h"
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

// ── AI Overlay model ──────────────────────────────────────────────────────

// Forward declaration for YOLO results
struct YoloDetection;

struct AiOverlayModel {
    bool show_detection_boxes = true;
    bool show_segmentation_overlay = true;
    float seg_alpha = 0.4f;
    const std::vector<DetectionBox>* detections = nullptr;
    const SegmentationMask* segmentation = nullptr;
    const ClassificationResult* classification = nullptr;
    const std::vector<AiLabel>* labels = nullptr;
    // YOLO normalized-coordinate detections (optional, takes priority)
    const std::vector<YoloDetection>* yolo_detections = nullptr;
};

class OverlayRenderer {
public:
    void DrawMeasurementOverlay(
        HDC hdc,
        const RECT& viewport,
        const ImageFrame& frame,
        ImageViewport& image_viewport,
        const MeasurementOverlayModel& model) const;

    // ── AI Overlay rendering ────────────────────────────────────────────

    void DrawAiOverlay(
        HDC hdc,
        const RECT& viewport,
        const ImageFrame& frame,
        ImageViewport& image_viewport,
        const AiOverlayModel& model) const;

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

    // AI drawing helpers
    void DrawDetectionBoxes(
        HDC hdc,
        const RECT& viewport,
        ImageViewport& image_viewport,
        const std::vector<DetectionBox>& detections,
        const std::vector<AiLabel>& labels) const;

    void DrawYoloDetectionBoxes(
        HDC hdc,
        const RECT& image_rect,
        const ImageFrame& frame,
        const std::vector<YoloDetection>& detections,
        const std::vector<AiLabel>& labels) const;

    void DrawSegmentationOverlay(
        HDC hdc,
        const RECT& viewport,
        const ImageFrame& frame,
        ImageViewport& image_viewport,
        const SegmentationMask& mask,
        float alpha) const;

    void DrawClassificationLabel(
        HDC hdc,
        const RECT& viewport,
        ImageViewport& image_viewport,
        const ClassificationResult& cls) const;

    static COLORREF GetLabelColorRef(int label_id, const std::vector<AiLabel>& labels);
};
