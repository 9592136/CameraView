#include "MeasurementFormatter.h"

#include <iomanip>
#include <sstream>

namespace {

MeasurementUnit EffectiveUnit(const CalibrationProfile& calibration, MeasurementUnit requested)
{
    return calibration.IsCalibrated() || requested == MeasurementUnit::Pixels
        ? requested : MeasurementUnit::Pixels;
}

double LinearValue(double pixels, const CalibrationProfile& calibration, MeasurementUnit requested)
{
    const MeasurementUnit unit = EffectiveUnit(calibration, requested);
    return unit == MeasurementUnit::Pixels ? pixels : calibration.PixelsToUnit(pixels, unit);
}

double AreaValue(double pixels, const CalibrationProfile& calibration, MeasurementUnit requested)
{
    const MeasurementUnit unit = EffectiveUnit(calibration, requested);
    if (unit == MeasurementUnit::Pixels) return pixels;
    const double squareMicrometers = pixels * calibration.MicronsPerPixel() * calibration.MicronsPerPixel();
    return unit == MeasurementUnit::Millimeters ? squareMicrometers / 1000000.0 : squareMicrometers;
}

std::wstring AreaLabel(MeasurementUnit unit)
{
    if (unit == MeasurementUnit::Micrometers) return L"um^2";
    if (unit == MeasurementUnit::Millimeters) return L"mm^2";
    return L"px^2";
}

} // namespace

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
    const MeasurementResult area = measurement.Evaluate(calibration, display_unit);
    const int precision = area.unit == MeasurementUnit::Pixels ? 1 : 2;
    return FormatResultLine(area) + L" · W " +
        FormatDouble(LinearValue(measurement.PixelWidth(), calibration, area.unit), precision) + L" " +
        CalibrationProfile::UnitLabel(area.unit) + L" · H " +
        FormatDouble(LinearValue(measurement.PixelHeight(), calibration, area.unit), precision) + L" " +
        CalibrationProfile::UnitLabel(area.unit) + L" · P " +
        FormatDouble(LinearValue(measurement.PixelPerimeter(), calibration, area.unit), precision) + L" " +
        CalibrationProfile::UnitLabel(area.unit);
}

std::wstring MeasurementFormatter::FormatLine(
    const PointMeasurement& measurement,
    const CalibrationProfile& calibration,
    MeasurementUnit display_unit)
{
    const MeasurementUnit unit = EffectiveUnit(calibration, display_unit);
    const int precision = unit == MeasurementUnit::Pixels ? 1 : 2;
    return measurement.Name() + L": X " +
        FormatDouble(LinearValue(measurement.Point().x, calibration, unit), precision) + L" " +
        CalibrationProfile::UnitLabel(unit) + L" · Y " +
        FormatDouble(LinearValue(measurement.Point().y, calibration, unit), precision) + L" " +
        CalibrationProfile::UnitLabel(unit);
}

std::wstring MeasurementFormatter::FormatLine(
    const PolylineMeasurement& measurement,
    const CalibrationProfile& calibration,
    MeasurementUnit display_unit)
{
    return FormatResultLine(measurement.Evaluate(calibration, display_unit));
}

std::wstring MeasurementFormatter::FormatLine(
    const CircleMeasurement& measurement,
    const CalibrationProfile& calibration,
    MeasurementUnit display_unit)
{
    const MeasurementUnit unit = EffectiveUnit(calibration, display_unit);
    const int precision = unit == MeasurementUnit::Pixels ? 1 : 2;
    const std::wstring linearLabel = CalibrationProfile::UnitLabel(unit);
    return measurement.Name() + L": A " +
        FormatDouble(AreaValue(measurement.PixelArea(), calibration, unit), precision) + L" " + AreaLabel(unit) +
        L" · R " + FormatDouble(LinearValue(measurement.PixelRadius(), calibration, unit), precision) + L" " + linearLabel +
        L" · D " + FormatDouble(LinearValue(measurement.PixelDiameter(), calibration, unit), precision) + L" " + linearLabel +
        L" · C " + FormatDouble(LinearValue(measurement.PixelCircumference(), calibration, unit), precision) + L" " + linearLabel;
}

std::wstring MeasurementFormatter::FormatLine(
    const EllipseMeasurement& measurement,
    const CalibrationProfile& calibration,
    MeasurementUnit display_unit)
{
    const MeasurementUnit unit = EffectiveUnit(calibration, display_unit);
    const int precision = unit == MeasurementUnit::Pixels ? 1 : 2;
    const std::wstring linearLabel = CalibrationProfile::UnitLabel(unit);
    return measurement.Name() + L": A " +
        FormatDouble(AreaValue(measurement.PixelArea(), calibration, unit), precision) + L" " + AreaLabel(unit) +
        L" · W " + FormatDouble(LinearValue(measurement.PixelWidth(), calibration, unit), precision) + L" " + linearLabel +
        L" · H " + FormatDouble(LinearValue(measurement.PixelHeight(), calibration, unit), precision) + L" " + linearLabel +
        L" · P " + FormatDouble(LinearValue(measurement.PixelPerimeter(), calibration, unit), precision) + L" " + linearLabel;
}

std::wstring MeasurementFormatter::FormatLine(
    const PolygonAreaMeasurement& measurement,
    const CalibrationProfile& calibration,
    MeasurementUnit display_unit)
{
    const MeasurementResult area = measurement.Evaluate(calibration, display_unit);
    const int precision = area.unit == MeasurementUnit::Pixels ? 1 : 2;
    return FormatResultLine(area) + L" · P " +
        FormatDouble(LinearValue(measurement.PixelPerimeter(), calibration, area.unit), precision) + L" " +
        CalibrationProfile::UnitLabel(area.unit);
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
    for (const PointMeasurement& measurement : measurements.Points()) {
        lines.push_back(FormatLine(measurement, calibration, display_unit));
    }
    for (const PolylineMeasurement& measurement : measurements.Polylines()) {
        lines.push_back(FormatLine(measurement, calibration, display_unit));
    }
    for (const CircleMeasurement& measurement : measurements.Circles()) {
        lines.push_back(FormatLine(measurement, calibration, display_unit));
    }
    for (const EllipseMeasurement& measurement : measurements.Ellipses()) {
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
