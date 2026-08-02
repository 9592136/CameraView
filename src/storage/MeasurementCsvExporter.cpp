#include "MeasurementCsvExporter.h"
#include "../domain/MeasurementFormatter.h"

#include <windows.h>

#include <fstream>
#include <iomanip>
#include <sstream>

namespace {

std::string FormatCsvNumber(double value, int precision)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

std::string WideToUtf8(const std::wstring& value)
{
    if (value.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (size <= 0) {
        return {};
    }

    std::string output(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        output.data(),
        size,
        nullptr,
        nullptr);
    return output;
}

std::string CsvEscape(std::string value)
{
    const bool needs_quotes = value.find_first_of(",\"\r\n") != std::string::npos;
    if (!needs_quotes) {
        return value;
    }

    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (char ch : value) {
        if (ch == '"') {
            escaped.push_back('"');
        }
        escaped.push_back(ch);
    }
    escaped.push_back('"');
    return escaped;
}

std::string CsvEscape(const std::wstring& value)
{
    return CsvEscape(WideToUtf8(value));
}

std::string FormatPointListCsv(const std::vector<ImagePoint>& points)
{
    std::ostringstream stream;
    for (std::size_t index = 0; index < points.size(); ++index) {
        if (index > 0) {
            stream << ';';
        }
        stream << FormatCsvNumber(points[index].x, 4)
               << ':'
               << FormatCsvNumber(points[index].y, 4);
    }
    return stream.str();
}

void WriteLengthRow(
    std::ofstream& output,
    const LengthMeasurement& measurement,
    const CalibrationProfile& calibration,
    MeasurementUnit display_unit,
    const std::wstring& objective_label)
{
    const MeasurementResult result = measurement.Evaluate(calibration, display_unit);
    const ImagePoint first = measurement.First();
    const ImagePoint second = measurement.Second();
    output << CsvEscape(result.name) << ','
           << CsvEscape(result.kind) << ','
           << FormatCsvNumber(result.calibrated_value, result.unit == MeasurementUnit::Pixels ? 1 : 4) << ','
           << CsvEscape(result.unit_label) << ','
           << FormatCsvNumber(result.pixel_value, 4) << ','
           << FormatCsvNumber(first.x, 4) << ','
           << FormatCsvNumber(first.y, 4) << ','
           << FormatCsvNumber(second.x, 4) << ','
           << FormatCsvNumber(second.y, 4) << ','
           << ','
           << ','
           << ','
           << CsvEscape(objective_label) << ','
           << FormatCsvNumber(calibration.MicronsPerPixel(), 8) << ','
           << CsvEscape(MeasurementFormatter::FormatLine(measurement, calibration, display_unit)) << '\n';
}

void WriteAngleRow(
    std::ofstream& output,
    const AngleMeasurement& measurement,
    const CalibrationProfile& calibration,
    const std::wstring& objective_label)
{
    const MeasurementResult result = measurement.Evaluate();
    const ImagePoint first = measurement.First();
    const ImagePoint vertex = measurement.Vertex();
    const ImagePoint second = measurement.Second();
    output << CsvEscape(result.name) << ','
           << CsvEscape(result.kind) << ','
           << FormatCsvNumber(result.calibrated_value, 2) << ','
           << CsvEscape(result.unit_label) << ','
           << FormatCsvNumber(result.pixel_value, 4) << ','
           << FormatCsvNumber(first.x, 4) << ','
           << FormatCsvNumber(first.y, 4) << ','
           << FormatCsvNumber(vertex.x, 4) << ','
           << FormatCsvNumber(vertex.y, 4) << ','
           << FormatCsvNumber(second.x, 4) << ','
           << FormatCsvNumber(second.y, 4) << ','
           << ','
           << CsvEscape(objective_label) << ','
           << FormatCsvNumber(calibration.MicronsPerPixel(), 8) << ','
           << CsvEscape(MeasurementFormatter::FormatLine(measurement)) << '\n';
}

void WriteRectangleRow(
    std::ofstream& output,
    const RectangleAreaMeasurement& measurement,
    const CalibrationProfile& calibration,
    MeasurementUnit display_unit,
    const std::wstring& objective_label)
{
    const MeasurementResult result = measurement.Evaluate(calibration, display_unit);
    const ImagePoint first = measurement.First();
    const ImagePoint second = measurement.Second();
    output << CsvEscape(result.name) << ','
           << CsvEscape(result.kind) << ','
           << FormatCsvNumber(result.calibrated_value, result.unit == MeasurementUnit::Pixels ? 1 : 4) << ','
           << CsvEscape(result.unit_label) << ','
           << FormatCsvNumber(result.pixel_value, 4) << ','
           << FormatCsvNumber(first.x, 4) << ','
           << FormatCsvNumber(first.y, 4) << ','
           << FormatCsvNumber(second.x, 4) << ','
           << FormatCsvNumber(second.y, 4) << ','
           << ','
           << ','
           << ','
           << CsvEscape(objective_label) << ','
           << FormatCsvNumber(calibration.MicronsPerPixel(), 8) << ','
           << CsvEscape(MeasurementFormatter::FormatLine(measurement, calibration, display_unit)) << '\n';
}

void WritePolygonRow(
    std::ofstream& output,
    const PolygonAreaMeasurement& measurement,
    const CalibrationProfile& calibration,
    MeasurementUnit display_unit,
    const std::wstring& objective_label)
{
    const MeasurementResult result = measurement.Evaluate(calibration, display_unit);
    const std::vector<ImagePoint>& points = measurement.Points();
    output << CsvEscape(result.name) << ','
           << CsvEscape(result.kind) << ','
           << FormatCsvNumber(result.calibrated_value, result.unit == MeasurementUnit::Pixels ? 1 : 4) << ','
           << CsvEscape(result.unit_label) << ','
           << FormatCsvNumber(result.pixel_value, 4) << ',';
    for (std::size_t index = 0; index < 3; ++index) {
        if (index < points.size()) {
            output << FormatCsvNumber(points[index].x, 4) << ','
                   << FormatCsvNumber(points[index].y, 4) << ',';
        } else {
            output << ','
                   << ',';
        }
    }
    output << CsvEscape(FormatPointListCsv(points)) << ','
           << CsvEscape(objective_label) << ','
           << FormatCsvNumber(calibration.MicronsPerPixel(), 8) << ','
           << CsvEscape(MeasurementFormatter::FormatLine(measurement, calibration, display_unit)) << '\n';
}

void WritePointRow(std::ofstream& output, const PointMeasurement& measurement,
    const CalibrationProfile& calibration, MeasurementUnit displayUnit, const std::wstring& objective)
{
    const ImagePoint point = measurement.Point();
    output << CsvEscape(measurement.Name()) << ",Point,,px,,"
           << FormatCsvNumber(point.x, 4) << ',' << FormatCsvNumber(point.y, 4)
           << ",,,,,," << CsvEscape(objective) << ','
           << FormatCsvNumber(calibration.MicronsPerPixel(), 8) << ','
           << CsvEscape(MeasurementFormatter::FormatLine(measurement, calibration, displayUnit)) << '\n';
}

void WritePolylineRow(std::ofstream& output, const PolylineMeasurement& measurement,
    const CalibrationProfile& calibration, MeasurementUnit displayUnit, const std::wstring& objective)
{
    const MeasurementResult result = measurement.Evaluate(calibration, displayUnit);
    const auto& points = measurement.Points();
    output << CsvEscape(measurement.Name()) << ",Polyline,"
           << FormatCsvNumber(result.calibrated_value, result.unit == MeasurementUnit::Pixels ? 1 : 4) << ','
           << CsvEscape(result.unit_label) << ',' << FormatCsvNumber(result.pixel_value, 4) << ',';
    for (std::size_t index = 0; index < 3; ++index) {
        if (index < points.size()) output << FormatCsvNumber(points[index].x, 4) << ',' << FormatCsvNumber(points[index].y, 4) << ',';
        else output << ",,";
    }
    output << CsvEscape(FormatPointListCsv(points)) << ',' << CsvEscape(objective) << ','
           << FormatCsvNumber(calibration.MicronsPerPixel(), 8) << ','
           << CsvEscape(MeasurementFormatter::FormatLine(measurement, calibration, displayUnit)) << '\n';
}

template <typename Shape>
void WriteAreaShapeRow(std::ofstream& output, const Shape& measurement, const char* kind,
    ImagePoint first, ImagePoint second, const CalibrationProfile& calibration,
    MeasurementUnit displayUnit, const std::wstring& objective)
{
    const bool calibrated = calibration.IsCalibrated() && displayUnit != MeasurementUnit::Pixels;
    const MeasurementUnit unit = calibrated ? displayUnit : MeasurementUnit::Pixels;
    double value = measurement.PixelArea();
    if (unit != MeasurementUnit::Pixels) {
        value *= calibration.MicronsPerPixel() * calibration.MicronsPerPixel();
        if (unit == MeasurementUnit::Millimeters) value /= 1000000.0;
    }
    const std::wstring unitLabel = unit == MeasurementUnit::Micrometers ? L"um^2" :
        (unit == MeasurementUnit::Millimeters ? L"mm^2" : L"px^2");
    output << CsvEscape(measurement.Name()) << ',' << kind << ',' << FormatCsvNumber(value, unit == MeasurementUnit::Pixels ? 1 : 4) << ','
           << CsvEscape(unitLabel) << ',' << FormatCsvNumber(measurement.PixelArea(), 4) << ','
           << FormatCsvNumber(first.x, 4) << ',' << FormatCsvNumber(first.y, 4) << ','
           << FormatCsvNumber(second.x, 4) << ',' << FormatCsvNumber(second.y, 4)
           << ",,,," << CsvEscape(objective) << ',' << FormatCsvNumber(calibration.MicronsPerPixel(), 8) << ','
           << CsvEscape(MeasurementFormatter::FormatLine(measurement, calibration, displayUnit)) << '\n';
}

} // namespace

bool MeasurementCsvExporter::Save(
    const std::filesystem::path& path,
    const MeasurementCollection& measurements,
    const CalibrationProfile& calibration,
    MeasurementUnit display_unit,
    const std::wstring& objective_label,
    std::wstring& error)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        error = L"Failed to create CSV file.";
        return false;
    }

    output << "\xEF\xBB\xBF";
    output << "Name,Kind,Value,Unit,RawPixelValue,Point1X,Point1Y,Point2X,Point2Y,Point3X,Point3Y,Points,Objective,MicronsPerPixel,Metrics\n";
    for (const LengthMeasurement& measurement : measurements.Lengths()) {
        WriteLengthRow(output, measurement, calibration, display_unit, objective_label);
    }
    for (const AngleMeasurement& measurement : measurements.Angles()) {
        WriteAngleRow(output, measurement, calibration, objective_label);
    }
    for (const RectangleAreaMeasurement& measurement : measurements.Rectangles()) {
        WriteRectangleRow(output, measurement, calibration, display_unit, objective_label);
    }
    for (const PolygonAreaMeasurement& measurement : measurements.Polygons()) {
        WritePolygonRow(output, measurement, calibration, display_unit, objective_label);
    }
    for (const PointMeasurement& measurement : measurements.Points()) {
        WritePointRow(output, measurement, calibration, display_unit, objective_label);
    }
    for (const PolylineMeasurement& measurement : measurements.Polylines()) {
        WritePolylineRow(output, measurement, calibration, display_unit, objective_label);
    }
    for (const CircleMeasurement& measurement : measurements.Circles()) {
        WriteAreaShapeRow(output, measurement, "Circle", measurement.Center(), measurement.Edge(),
            calibration, display_unit, objective_label);
    }
    for (const EllipseMeasurement& measurement : measurements.Ellipses()) {
        WriteAreaShapeRow(output, measurement, "Ellipse", measurement.First(), measurement.Second(),
            calibration, display_unit, objective_label);
    }

    if (!output) {
        error = L"Failed while writing CSV file.";
        return false;
    }

    error.clear();
    return true;
}
