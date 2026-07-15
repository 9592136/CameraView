#include "MeasurementNameFormatter.h"

std::wstring MeasurementNameFormatter::FormatDefaultName(MeasurementKind kind, std::size_t one_based_index)
{
    return std::wstring(PrefixFor(kind)) + L" " + std::to_wstring(one_based_index);
}

std::wstring MeasurementNameFormatter::NextDefaultName(
    MeasurementKind kind,
    const MeasurementCollection& measurements)
{
    return FormatDefaultName(kind, CountFor(kind, measurements) + 1);
}

const wchar_t* MeasurementNameFormatter::PrefixFor(MeasurementKind kind)
{
    switch (kind) {
    case MeasurementKind::Length:
        return L"Length";
    case MeasurementKind::Angle:
        return L"Angle";
    case MeasurementKind::RectangleArea:
        return L"Area";
    case MeasurementKind::PolygonArea:
        return L"Polygon";
    case MeasurementKind::None:
        break;
    }
    return L"Measurement";
}

std::size_t MeasurementNameFormatter::CountFor(
    MeasurementKind kind,
    const MeasurementCollection& measurements)
{
    switch (kind) {
    case MeasurementKind::Length:
        return measurements.LengthCount();
    case MeasurementKind::Angle:
        return measurements.AngleCount();
    case MeasurementKind::RectangleArea:
        return measurements.RectangleCount();
    case MeasurementKind::PolygonArea:
        return measurements.PolygonCount();
    case MeasurementKind::None:
        break;
    }
    return measurements.Count();
}
