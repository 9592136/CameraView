#include "domain/MeasurementCollection.h"
#include "domain/MeasurementFormatter.h"
#include "domain/MeasurementNameFormatter.h"

#include <cmath>
#include <iostream>

namespace {

bool near(double lhs, double rhs, double tolerance = 1e-6)
{
    return std::abs(lhs - rhs) <= tolerance;
}

int fail(const char* message)
{
    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main()
{
    const CalibrationProfile calibration = CalibrationProfile::FromMicronsPerPixel(2.0);

    RectangleAreaMeasurement rectangle(L"Rectangle", {1.0, 2.0}, {4.0, 6.0});
    if (!near(rectangle.PixelWidth(), 3.0) || !near(rectangle.PixelHeight(), 4.0) ||
        !near(rectangle.PixelPerimeter(), 14.0) || !near(rectangle.PixelArea(), 12.0)) {
        return fail("Rectangle metrics are incorrect.");
    }

    PolygonAreaMeasurement polygon(L"Polygon", {{0.0, 0.0}, {3.0, 0.0}, {3.0, 4.0}});
    if (!near(polygon.PixelArea(), 6.0) || !near(polygon.PixelPerimeter(), 12.0)) {
        return fail("Polygon area or perimeter is incorrect.");
    }

    PolylineMeasurement polyline(L"Polyline", {{0.0, 0.0}, {3.0, 4.0}, {6.0, 4.0}});
    if (!near(polyline.PixelLength(), 8.0) ||
        !near(polyline.Evaluate(calibration, MeasurementUnit::Micrometers).calibrated_value, 16.0)) {
        return fail("Polyline length or calibration is incorrect.");
    }

    CircleMeasurement circle(L"Circle", {10.0, 10.0}, {13.0, 14.0});
    if (!near(circle.PixelRadius(), 5.0) || !near(circle.PixelDiameter(), 10.0) ||
        !near(circle.PixelCircumference(), 10.0 * 3.14159265358979323846) ||
        !near(circle.PixelArea(), 25.0 * 3.14159265358979323846)) {
        return fail("Circle metrics are incorrect.");
    }

    EllipseMeasurement ellipse(L"Ellipse", {0.0, 0.0}, {10.0, 6.0});
    if (!near(ellipse.PixelWidth(), 10.0) || !near(ellipse.PixelHeight(), 6.0) ||
        !near(ellipse.PixelArea(), 15.0 * 3.14159265358979323846) || ellipse.PixelPerimeter() <= 25.0) {
        return fail("Ellipse metrics are incorrect.");
    }

    PointMeasurement point(L"Point", {4.0, 7.0});
    const std::wstring pointLine = MeasurementFormatter::FormatLine(
        point, calibration, MeasurementUnit::Micrometers);
    const std::wstring rectangleLine = MeasurementFormatter::FormatLine(
        rectangle, calibration, MeasurementUnit::Micrometers);
    const std::wstring circleLine = MeasurementFormatter::FormatLine(
        circle, calibration, MeasurementUnit::Micrometers);
    if (pointLine.find(L"X 8.00 um") == std::wstring::npos ||
        rectangleLine.find(L"W 6.00 um") == std::wstring::npos ||
        rectangleLine.find(L"P 28.00 um") == std::wstring::npos ||
        circleLine.find(L"R 10.00 um") == std::wstring::npos ||
        circleLine.find(L"D 20.00 um") == std::wstring::npos) {
        return fail("Extended measurement formatting is incomplete.");
    }

    MeasurementCollection collection;
    collection.AddLength(L"L", {0.0, 0.0}, {1.0, 0.0});
    collection.AddAngle(L"A", {1.0, 0.0}, {0.0, 0.0}, {0.0, 1.0});
    collection.AddRectangleArea(L"R", {0.0, 0.0}, {1.0, 1.0});
    collection.AddPolygonArea(L"P", {{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}});
    collection.AddPoint(L"Pt", {2.0, 3.0});
    collection.AddPolyline(L"Pl", {{0.0, 0.0}, {1.0, 1.0}});
    collection.AddCircle(L"C", {0.0, 0.0}, {2.0, 0.0});
    collection.AddEllipse(L"E", {0.0, 0.0}, {4.0, 2.0});
    if (collection.Count() != 8 || collection.AtFlatIndex(3)->kind != MeasurementKind::PolygonArea ||
        collection.AtFlatIndex(4)->kind != MeasurementKind::Point ||
        collection.AtFlatIndex(7)->kind != MeasurementKind::Ellipse ||
        collection.FlatIndexOf({MeasurementKind::Circle, 0}) != 6 ||
        MeasurementNameFormatter::NextDefaultName(MeasurementKind::Circle, collection) != L"Circle 2") {
        return fail("Extended collection indexing or naming is incorrect.");
    }
    if (!collection.SetPoint({MeasurementKind::Point, 0}, EditablePoint::First, 0, {9.0, 8.0}) ||
        !near(collection.Points()[0].Point().x, 9.0) || !collection.EraseAtFlatIndex(5) ||
        collection.PolylineCount() != 0 || collection.Count() != 7) {
        return fail("Extended collection editing or deletion is incorrect.");
    }

    return 0;
}
