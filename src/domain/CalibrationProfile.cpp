#include "CalibrationProfile.h"

#include <algorithm>
#include <cmath>

namespace {

double ToMicrometers(double value, MeasurementUnit unit)
{
    switch (unit) {
    case MeasurementUnit::Pixels:
        return 0.0;
    case MeasurementUnit::Micrometers:
        return value;
    case MeasurementUnit::Millimeters:
        return value * 1000.0;
    default:
        return 0.0;
    }
}

} // namespace

CalibrationProfile CalibrationProfile::Uncalibrated()
{
    return CalibrationProfile(0.0);
}

CalibrationProfile CalibrationProfile::FromMicronsPerPixel(double microns_per_pixel)
{
    if (!std::isfinite(microns_per_pixel) || microns_per_pixel <= 0.0) {
        return Uncalibrated();
    }
    return CalibrationProfile(microns_per_pixel);
}

CalibrationProfile CalibrationProfile::FromTwoPointCalibration(
    ImagePoint first,
    ImagePoint second,
    double real_length,
    MeasurementUnit real_length_unit)
{
    const double pixels = DistancePixels(first, second);
    const double micrometers = ToMicrometers(real_length, real_length_unit);
    if (!std::isfinite(pixels) || pixels <= 0.0 || !std::isfinite(micrometers) || micrometers <= 0.0) {
        return Uncalibrated();
    }
    return CalibrationProfile(micrometers / pixels);
}

double CalibrationProfile::PixelsToMicrometers(double pixels) const
{
    if (!IsCalibrated()) {
        return 0.0;
    }
    return pixels * microns_per_pixel_;
}

double CalibrationProfile::PixelsToUnit(double pixels, MeasurementUnit unit) const
{
    switch (unit) {
    case MeasurementUnit::Pixels:
        return pixels;
    case MeasurementUnit::Micrometers:
        return PixelsToMicrometers(pixels);
    case MeasurementUnit::Millimeters:
        return PixelsToMicrometers(pixels) / 1000.0;
    default:
        return pixels;
    }
}

const std::vector<MeasurementUnit>& CalibrationProfile::CalibrationUnitOptions()
{
    static const std::vector<MeasurementUnit> units = {
        MeasurementUnit::Micrometers,
        MeasurementUnit::Millimeters
    };
    return units;
}

MeasurementUnit CalibrationProfile::CalibrationUnitAtIndex(int index)
{
    const std::vector<MeasurementUnit>& units = CalibrationUnitOptions();
    if (index < 0 || index >= static_cast<int>(units.size())) {
        return MeasurementUnit::Micrometers;
    }
    return units[static_cast<std::size_t>(index)];
}

std::wstring CalibrationProfile::UnitLabel(MeasurementUnit unit)
{
    switch (unit) {
    case MeasurementUnit::Pixels:
        return L"px";
    case MeasurementUnit::Micrometers:
        return L"um";
    case MeasurementUnit::Millimeters:
        return L"mm";
    default:
        return L"";
    }
}

const std::vector<std::wstring>& CalibrationProfile::ObjectiveMagnificationOptions()
{
    static const std::vector<std::wstring> objectives = {
        L"4x",
        L"10x",
        L"20x",
        L"40x",
        L"60x",
        L"100x"
    };
    return objectives;
}

int CalibrationProfile::ObjectiveIndexAtSelection(int index)
{
    const std::vector<std::wstring>& objectives = ObjectiveMagnificationOptions();
    if (index < 0 || index >= static_cast<int>(objectives.size())) {
        return 0;
    }
    return index;
}

int CalibrationProfile::ObjectiveIndexForLabel(const std::wstring& label)
{
    const std::vector<std::wstring>& objectives = ObjectiveMagnificationOptions();
    const auto match = std::find(objectives.begin(), objectives.end(), label);
    if (match == objectives.end()) {
        return -1;
    }
    return static_cast<int>(std::distance(objectives.begin(), match));
}

std::wstring CalibrationProfile::ObjectiveLabelAtIndex(int index)
{
    const std::vector<std::wstring>& objectives = ObjectiveMagnificationOptions();
    return objectives[static_cast<std::size_t>(ObjectiveIndexAtSelection(index))];
}
