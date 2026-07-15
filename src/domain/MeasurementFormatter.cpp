#include "MeasurementFormatter.h"

#include <iomanip>
#include <sstream>

std::wstring MeasurementFormatter::FormatResultLine(const MeasurementResult& result)
{
    return result.name + L": " +
           FormatDouble(result.calibrated_value, ValuePrecision(result)) +
           L" " + result.unit_label +
           L" (" + FormatDouble(result.pixel_value, 1) + L" raw)";
}

std::wstring MeasurementFormatter::FormatLine(
    const LengthMeasurement& measurement,
    const CalibrationProfile& calibration,
    MeasurementUnit display_unit)
{
    return FormatResultLine(measurement.Evaluate(calibration, display_unit));
}

std::wstring MeasurementFormatter::FormatLine(const AngleMeasurement& measurement)
{
    return FormatResultLine(measurement.Evaluate());
}

std::wstring MeasurementFormatter::FormatLine(
    const RectangleAreaMeasurement& measurement,
    const CalibrationProfile& calibration,
    MeasurementUnit display_unit)
{
    return FormatResultLine(measurement.Evaluate(calibration, display_unit));
}

std::wstring MeasurementFormatter::FormatLine(
    const PolygonAreaMeasurement& measurement,
    const CalibrationProfile& calibration,
    MeasurementUnit display_unit)
{
    return FormatResultLine(measurement.Evaluate(calibration, display_unit));
}

std::vector<std::wstring> MeasurementFormatter::FormatCollection(
    const MeasurementCollection& measurements,
    const CalibrationProfile& calibration,
    MeasurementUnit display_unit)
{
    std::vector<std::wstring> lines;
    lines.reserve(measurements.Count());

    for (const LengthMeasurement& measurement : measurements.Lengths()) {
        lines.push_back(FormatLine(measurement, calibration, display_unit));
    }
    for (const AngleMeasurement& measurement : measurements.Angles()) {
        lines.push_back(FormatLine(measurement));
    }
    for (const RectangleAreaMeasurement& measurement : measurements.Rectangles()) {
        lines.push_back(FormatLine(measurement, calibration, display_unit));
    }
    for (const PolygonAreaMeasurement& measurement : measurements.Polygons()) {
        lines.push_back(FormatLine(measurement, calibration, display_unit));
    }

    return lines;
}

std::wstring MeasurementFormatter::FormatDouble(double value, int precision)
{
    std::wostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

int MeasurementFormatter::ValuePrecision(const MeasurementResult& result)
{
    if (result.kind == L"Angle") {
        return 2;
    }
    return result.unit == MeasurementUnit::Pixels ? 1 : 2;
}
