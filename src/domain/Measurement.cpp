#include "Measurement.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr double kPi = 3.14159265358979323846;

std::wstring AreaUnitLabel(MeasurementUnit unit)
{
    switch (unit) {
    case MeasurementUnit::Micrometers:
        return L"um^2";
    case MeasurementUnit::Millimeters:
        return L"mm^2";
    case MeasurementUnit::Pixels:
    default:
        return L"px^2";
    }
}

MeasurementResult BuildAreaResult(
    const std::wstring& name,
    double pixel_area,
    const CalibrationProfile& calibration,
    MeasurementUnit output_unit)
{
    MeasurementResult result;
    result.name = name;
    result.kind = L"Area";
    result.pixel_value = pixel_area;
    result.unit = calibration.IsCalibrated() || output_unit == MeasurementUnit::Pixels
        ? output_unit
        : MeasurementUnit::Pixels;
    if (result.unit == MeasurementUnit::Pixels) {
        result.calibrated_value = result.pixel_value;
    } else {
        const double micrometers_per_pixel = calibration.MicronsPerPixel();
        const double square_micrometers = result.pixel_value * micrometers_per_pixel * micrometers_per_pixel;
        result.calibrated_value = result.unit == MeasurementUnit::Millimeters
            ? square_micrometers / 1000000.0
            : square_micrometers;
    }
    result.unit_label = AreaUnitLabel(result.unit);
    return result;
}

} // namespace

LengthMeasurement::LengthMeasurement(std::wstring name, ImagePoint first, ImagePoint second)
    : name_(std::move(name)), first_(first), second_(second)
{
}

void LengthMeasurement::SetName(std::wstring name)
{
    name_ = std::move(name);
}

void LengthMeasurement::SetFirst(ImagePoint point)
{
    first_ = point;
}

void LengthMeasurement::SetSecond(ImagePoint point)
{
    second_ = point;
}

double LengthMeasurement::PixelLength() const
{
    return DistancePixels(first_, second_);
}

MeasurementResult LengthMeasurement::Evaluate(const CalibrationProfile& calibration, MeasurementUnit output_unit) const
{
    MeasurementResult result;
    result.name = name_;
    result.kind = L"Length";
    result.pixel_value = PixelLength();
    result.unit = calibration.IsCalibrated() || output_unit == MeasurementUnit::Pixels
        ? output_unit
        : MeasurementUnit::Pixels;
    result.calibrated_value = calibration.PixelsToUnit(result.pixel_value, output_unit);
    if (result.unit == MeasurementUnit::Pixels) {
        result.calibrated_value = result.pixel_value;
    }
    result.unit_label = CalibrationProfile::UnitLabel(result.unit);
    return result;
}

AngleMeasurement::AngleMeasurement(std::wstring name, ImagePoint first, ImagePoint vertex, ImagePoint second)
    : name_(std::move(name)), first_(first), vertex_(vertex), second_(second)
{
}

void AngleMeasurement::SetName(std::wstring name)
{
    name_ = std::move(name);
}

void AngleMeasurement::SetFirst(ImagePoint point)
{
    first_ = point;
}

void AngleMeasurement::SetVertex(ImagePoint point)
{
    vertex_ = point;
}

void AngleMeasurement::SetSecond(ImagePoint point)
{
    second_ = point;
}

double AngleMeasurement::Degrees() const
{
    const double ax = first_.x - vertex_.x;
    const double ay = first_.y - vertex_.y;
    const double bx = second_.x - vertex_.x;
    const double by = second_.y - vertex_.y;
    const double a_length = std::hypot(ax, ay);
    const double b_length = std::hypot(bx, by);
    if (a_length <= 0.0 || b_length <= 0.0) {
        return 0.0;
    }

    const double dot = ax * bx + ay * by;
    const double cosine = std::clamp(dot / (a_length * b_length), -1.0, 1.0);
    return std::acos(cosine) * 180.0 / kPi;
}

MeasurementResult AngleMeasurement::Evaluate() const
{
    MeasurementResult result;
    result.name = name_;
    result.kind = L"Angle";
    result.pixel_value = Degrees();
    result.calibrated_value = result.pixel_value;
    result.unit = MeasurementUnit::Pixels;
    result.unit_label = L"deg";
    return result;
}

RectangleAreaMeasurement::RectangleAreaMeasurement(std::wstring name, ImagePoint first, ImagePoint second)
    : name_(std::move(name)), first_(first), second_(second)
{
}

void RectangleAreaMeasurement::SetName(std::wstring name)
{
    name_ = std::move(name);
}

void RectangleAreaMeasurement::SetFirst(ImagePoint point)
{
    first_ = point;
}

void RectangleAreaMeasurement::SetSecond(ImagePoint point)
{
    second_ = point;
}

double RectangleAreaMeasurement::PixelArea() const
{
    return std::abs((second_.x - first_.x) * (second_.y - first_.y));
}

MeasurementResult RectangleAreaMeasurement::Evaluate(const CalibrationProfile& calibration, MeasurementUnit output_unit) const
{
    return BuildAreaResult(name_, PixelArea(), calibration, output_unit);
}

PolygonAreaMeasurement::PolygonAreaMeasurement(std::wstring name, std::vector<ImagePoint> points)
    : name_(std::move(name)), points_(std::move(points))
{
}

void PolygonAreaMeasurement::SetName(std::wstring name)
{
    name_ = std::move(name);
}

void PolygonAreaMeasurement::SetPoints(std::vector<ImagePoint> points)
{
    points_ = std::move(points);
}

void PolygonAreaMeasurement::SetPoint(std::size_t index, ImagePoint point)
{
    if (index < points_.size()) {
        points_[index] = point;
    }
}

double PolygonAreaMeasurement::PixelArea() const
{
    if (points_.size() < 3) {
        return 0.0;
    }

    double twice_area = 0.0;
    for (std::size_t index = 0; index < points_.size(); ++index) {
        const ImagePoint& current = points_[index];
        const ImagePoint& next = points_[(index + 1) % points_.size()];
        twice_area += current.x * next.y - next.x * current.y;
    }
    return std::abs(twice_area) * 0.5;
}

MeasurementResult PolygonAreaMeasurement::Evaluate(const CalibrationProfile& calibration, MeasurementUnit output_unit) const
{
    return BuildAreaResult(name_, PixelArea(), calibration, output_unit);
}
