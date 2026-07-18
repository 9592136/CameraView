#include "MeasurementActionApplier.h"

#include "../domain/MeasurementFormatter.h"
#include "../domain/MeasurementNameFormatter.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <utility>

namespace {

double CalibrationLengthMicrometers(double length, MeasurementUnit unit)
{
    switch (unit) {
    case MeasurementUnit::Micrometers:
        return length;
    case MeasurementUnit::Millimeters:
        return length * 1000.0;
    case MeasurementUnit::Pixels:
    default:
        return 0.0;
    }
}

} // namespace

MeasurementActionApplyResult MeasurementActionApplier::Apply(
    MeasurementInteractionAction action,
    MeasurementCollection& measurements,
    CalibrationProfile& calibration,
    double calibration_length,
    MeasurementUnit calibration_unit,
    MeasurementUnit display_unit)
{
    MeasurementActionApplyResult result;

    switch (action.kind) {
    case MeasurementInteractionActionKind::Prompt:
    case MeasurementInteractionActionKind::PolygonPointAdded:
    case MeasurementInteractionActionKind::PolygonInactive:
    case MeasurementInteractionActionKind::PolygonTooFew:
        result.status = action.status;
        return result;
    case MeasurementInteractionActionKind::CalibrationCompleted:
        if (DistancePixels(action.first, action.second) <= 0.0) {
            result.preview_changed = true;
            result.status = L"Calibration failed: click two different points in the image.";
            return result;
        }
        calibration_length = action.calibration_length;
        calibration_unit = action.calibration_unit;
        if (!std::isfinite(CalibrationLengthMicrometers(calibration_length, calibration_unit)) ||
            CalibrationLengthMicrometers(calibration_length, calibration_unit) <= 0.0) {
            result.preview_changed = true;
            result.status = L"Calibration failed: enter a positive scale length.";
            return result;
        }
        calibration = CalibrationProfile::FromTwoPointCalibration(
            action.first,
            action.second,
            calibration_length,
            calibration_unit);
        result.measurement_list_changed = calibration.IsCalibrated();
        result.preview_changed = true;
        result.calibration_changed = calibration.IsCalibrated();
        result.status = calibration.IsCalibrated()
            ? L"Calibration set to " + FormatDouble(calibration.MicronsPerPixel(), 4) + L" um/px."
            : L"Calibration failed: click two different points in the image.";
        return result;
    case MeasurementInteractionActionKind::LengthCompleted: {
        const std::wstring name = MeasurementNameFormatter::NextDefaultName(
            MeasurementKind::Length,
            measurements);
        const LengthMeasurement& measurement = measurements.AddLength(name, action.first, action.second);
        result.measurement_list_changed = true;
        result.preview_changed = true;
        result.status = MeasurementFormatter::FormatLine(measurement, calibration, display_unit);
        return result;
    }
    case MeasurementInteractionActionKind::AngleCompleted: {
        const std::wstring name = MeasurementNameFormatter::NextDefaultName(
            MeasurementKind::Angle,
            measurements);
        const AngleMeasurement& measurement = measurements.AddAngle(name, action.first, action.second, action.third);
        result.measurement_list_changed = true;
        result.preview_changed = true;
        result.status = MeasurementFormatter::FormatLine(measurement);
        return result;
    }
    case MeasurementInteractionActionKind::RectangleCompleted: {
        const std::wstring name = MeasurementNameFormatter::NextDefaultName(
            MeasurementKind::RectangleArea,
            measurements);
        const RectangleAreaMeasurement& measurement =
            measurements.AddRectangleArea(name, action.first, action.second);
        result.measurement_list_changed = true;
        result.preview_changed = true;
        result.status = MeasurementFormatter::FormatLine(measurement, calibration, display_unit);
        return result;
    }
    case MeasurementInteractionActionKind::PolygonCompleted: {
        const std::wstring name = MeasurementNameFormatter::NextDefaultName(
            MeasurementKind::PolygonArea,
            measurements);
        const PolygonAreaMeasurement& measurement =
            measurements.AddPolygonArea(name, std::move(action.polygon_points));
        result.measurement_list_changed = true;
        result.preview_changed = true;
        result.status = MeasurementFormatter::FormatLine(measurement, calibration, display_unit);
        return result;
    }
    case MeasurementInteractionActionKind::None:
    default:
        return result;
    }
}

std::wstring MeasurementActionApplier::FormatDouble(double value, int precision)
{
    std::wostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}
