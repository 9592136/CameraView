#pragma once

#include "Geometry.h"

#include <string>
#include <vector>

enum class MeasurementUnit {
    Pixels,
    Micrometers,
    Millimeters
};

class CalibrationProfile {
public:
    static CalibrationProfile Uncalibrated();
    static CalibrationProfile FromMicronsPerPixel(double microns_per_pixel);
    static CalibrationProfile FromTwoPointCalibration(
        ImagePoint first,
        ImagePoint second,
        double real_length,
        MeasurementUnit real_length_unit);

    bool IsCalibrated() const { return microns_per_pixel_ > 0.0; }
    double MicronsPerPixel() const { return microns_per_pixel_; }

    double PixelsToMicrometers(double pixels) const;
    double PixelsToUnit(double pixels, MeasurementUnit unit) const;

    static const std::vector<MeasurementUnit>& CalibrationUnitOptions();
    static MeasurementUnit CalibrationUnitAtIndex(int index);
    static std::wstring UnitLabel(MeasurementUnit unit);

private:
    explicit CalibrationProfile(double microns_per_pixel) : microns_per_pixel_(microns_per_pixel) {}

    double microns_per_pixel_ = 0.0;
};
