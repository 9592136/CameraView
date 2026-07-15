#pragma once

#include "../domain/CalibrationProfile.h"
#include "../domain/Geometry.h"

#include <string>
#include <vector>

enum class MeasurementInteractionMode {
    None,
    CalibrationFirstPoint,
    CalibrationSecondPoint,
    LengthFirstPoint,
    LengthSecondPoint,
    AngleFirstPoint,
    AngleVertexPoint,
    AngleSecondPoint,
    RectangleFirstPoint,
    RectangleSecondPoint,
    PolygonAreaCollecting
};

enum class MeasurementInteractionActionKind {
    None,
    Prompt,
    CalibrationCompleted,
    LengthCompleted,
    AngleCompleted,
    RectangleCompleted,
    PolygonPointAdded,
    PolygonCompleted,
    PolygonInactive,
    PolygonTooFew
};

enum class MeasurementInteractionPendingKind {
    None,
    Point,
    Angle,
    Polygon
};

struct MeasurementInteractionAction {
    MeasurementInteractionActionKind kind = MeasurementInteractionActionKind::None;
    std::wstring status;
    ImagePoint first;
    ImagePoint second;
    ImagePoint third;
    std::vector<ImagePoint> polygon_points;
    double calibration_length = 0.0;
    MeasurementUnit calibration_unit = MeasurementUnit::Micrometers;
};

struct MeasurementInteractionPending {
    MeasurementInteractionPendingKind kind = MeasurementInteractionPendingKind::None;
    ImagePoint first;
    ImagePoint second;
    const std::vector<ImagePoint>* polygon_points = nullptr;
};

class MeasurementInteractionState {
public:
    MeasurementInteractionMode Mode() const { return mode_; }
    bool IsIdle() const { return mode_ == MeasurementInteractionMode::None; }

    std::wstring BeginCalibration();
    std::wstring BeginCalibration(double length, MeasurementUnit unit);
    std::wstring BeginLength();
    std::wstring BeginAngle();
    std::wstring BeginRectangleArea();
    std::wstring BeginPolygonArea();
    void Clear();

    MeasurementInteractionAction AddPoint(ImagePoint point);
    MeasurementInteractionAction FinishPolygon();
    MeasurementInteractionPending PendingOverlay() const;

private:
    MeasurementInteractionMode mode_ = MeasurementInteractionMode::None;
    ImagePoint first_point_;
    ImagePoint second_point_;
    std::vector<ImagePoint> polygon_points_;
    double calibration_length_ = 0.0;
    MeasurementUnit calibration_unit_ = MeasurementUnit::Micrometers;
};
