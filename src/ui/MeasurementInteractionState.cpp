#include "MeasurementInteractionState.h"

namespace {

bool SamePoint(ImagePoint left, ImagePoint right)
{
    return left.x == right.x && left.y == right.y;
}

} // namespace

std::wstring MeasurementInteractionState::BeginCalibration()
{
    return BeginCalibration(100.0, MeasurementUnit::Micrometers);
}

std::wstring MeasurementInteractionState::BeginCalibration(double length, MeasurementUnit unit)
{
    Clear();
    calibration_length_ = length;
    calibration_unit_ = unit;
    mode_ = MeasurementInteractionMode::CalibrationFirstPoint;
    return L"Calibration: click the first point.";
}

std::wstring MeasurementInteractionState::BeginLength()
{
    Clear();
    mode_ = MeasurementInteractionMode::LengthFirstPoint;
    return L"Length: click the first point.";
}

std::wstring MeasurementInteractionState::BeginAngle()
{
    Clear();
    mode_ = MeasurementInteractionMode::AngleFirstPoint;
    return L"Angle: click the first ray point.";
}

std::wstring MeasurementInteractionState::BeginRectangleArea()
{
    Clear();
    mode_ = MeasurementInteractionMode::RectangleFirstPoint;
    return L"Area: click the first rectangle corner.";
}

std::wstring MeasurementInteractionState::BeginPolygonArea()
{
    Clear();
    mode_ = MeasurementInteractionMode::PolygonAreaCollecting;
    return L"Polygon: click vertices, then Finish Poly.";
}

void MeasurementInteractionState::Clear()
{
    mode_ = MeasurementInteractionMode::None;
    first_point_ = ImagePoint();
    second_point_ = ImagePoint();
    polygon_points_.clear();
    calibration_length_ = 0.0;
    calibration_unit_ = MeasurementUnit::Micrometers;
}

MeasurementInteractionAction MeasurementInteractionState::AddPoint(ImagePoint point)
{
    MeasurementInteractionAction action;
    switch (mode_) {
    case MeasurementInteractionMode::CalibrationFirstPoint:
        first_point_ = point;
        mode_ = MeasurementInteractionMode::CalibrationSecondPoint;
        action.kind = MeasurementInteractionActionKind::Prompt;
        action.status = L"Calibration: click the second point.";
        break;
    case MeasurementInteractionMode::CalibrationSecondPoint:
        if (SamePoint(first_point_, point)) {
            action.kind = MeasurementInteractionActionKind::Prompt;
            action.status = L"Calibration: click a different second point.";
            break;
        }
        action.kind = MeasurementInteractionActionKind::CalibrationCompleted;
        action.first = first_point_;
        action.second = point;
        action.calibration_length = calibration_length_;
        action.calibration_unit = calibration_unit_;
        Clear();
        break;
    case MeasurementInteractionMode::LengthFirstPoint:
        first_point_ = point;
        mode_ = MeasurementInteractionMode::LengthSecondPoint;
        action.kind = MeasurementInteractionActionKind::Prompt;
        action.status = L"Length: click the second point.";
        break;
    case MeasurementInteractionMode::LengthSecondPoint:
        action.kind = MeasurementInteractionActionKind::LengthCompleted;
        action.first = first_point_;
        action.second = point;
        Clear();
        break;
    case MeasurementInteractionMode::AngleFirstPoint:
        first_point_ = point;
        mode_ = MeasurementInteractionMode::AngleVertexPoint;
        action.kind = MeasurementInteractionActionKind::Prompt;
        action.status = L"Angle: click the vertex point.";
        break;
    case MeasurementInteractionMode::AngleVertexPoint:
        second_point_ = point;
        mode_ = MeasurementInteractionMode::AngleSecondPoint;
        action.kind = MeasurementInteractionActionKind::Prompt;
        action.status = L"Angle: click the second ray point.";
        break;
    case MeasurementInteractionMode::AngleSecondPoint:
        action.kind = MeasurementInteractionActionKind::AngleCompleted;
        action.first = first_point_;
        action.second = second_point_;
        action.third = point;
        Clear();
        break;
    case MeasurementInteractionMode::RectangleFirstPoint:
        first_point_ = point;
        mode_ = MeasurementInteractionMode::RectangleSecondPoint;
        action.kind = MeasurementInteractionActionKind::Prompt;
        action.status = L"Area: click the opposite rectangle corner.";
        break;
    case MeasurementInteractionMode::RectangleSecondPoint:
        action.kind = MeasurementInteractionActionKind::RectangleCompleted;
        action.first = first_point_;
        action.second = point;
        Clear();
        break;
    case MeasurementInteractionMode::PolygonAreaCollecting:
        polygon_points_.push_back(point);
        action.kind = MeasurementInteractionActionKind::PolygonPointAdded;
        action.status = L"Polygon: " + std::to_wstring(polygon_points_.size()) +
            L" vertices. Click more or Finish Poly.";
        break;
    case MeasurementInteractionMode::None:
    default:
        break;
    }
    return action;
}

MeasurementInteractionAction MeasurementInteractionState::FinishPolygon()
{
    MeasurementInteractionAction action;
    if (mode_ != MeasurementInteractionMode::PolygonAreaCollecting) {
        action.kind = MeasurementInteractionActionKind::PolygonInactive;
        action.status = L"Start Poly Area before finishing a polygon.";
        return action;
    }

    if (polygon_points_.size() < 3) {
        action.kind = MeasurementInteractionActionKind::PolygonTooFew;
        action.status = L"Polygon needs at least three vertices.";
        return action;
    }

    action.kind = MeasurementInteractionActionKind::PolygonCompleted;
    action.polygon_points = polygon_points_;
    Clear();
    return action;
}

MeasurementInteractionPending MeasurementInteractionState::PendingOverlay() const
{
    MeasurementInteractionPending pending;
    switch (mode_) {
    case MeasurementInteractionMode::CalibrationSecondPoint:
    case MeasurementInteractionMode::LengthSecondPoint:
    case MeasurementInteractionMode::AngleVertexPoint:
    case MeasurementInteractionMode::RectangleSecondPoint:
        pending.kind = MeasurementInteractionPendingKind::Point;
        pending.first = first_point_;
        break;
    case MeasurementInteractionMode::AngleSecondPoint:
        pending.kind = MeasurementInteractionPendingKind::Angle;
        pending.first = first_point_;
        pending.second = second_point_;
        break;
    case MeasurementInteractionMode::PolygonAreaCollecting:
        pending.kind = MeasurementInteractionPendingKind::Polygon;
        pending.polygon_points = &polygon_points_;
        break;
    case MeasurementInteractionMode::None:
    case MeasurementInteractionMode::CalibrationFirstPoint:
    case MeasurementInteractionMode::LengthFirstPoint:
    case MeasurementInteractionMode::AngleFirstPoint:
    case MeasurementInteractionMode::RectangleFirstPoint:
    default:
        break;
    }
    return pending;
}
