#pragma once

#include "CalibrationProfile.h"
#include "Measurement.h"
#include "MeasurementCollection.h"

#include <string>
#include <vector>

class MeasurementFormatter {
public:
    static std::wstring FormatResultLine(const MeasurementResult& result);
    static std::wstring FormatLine(
        const LengthMeasurement& measurement,
        const CalibrationProfile& calibration,
        MeasurementUnit display_unit);
    static std::wstring FormatLine(const AngleMeasurement& measurement);
    static std::wstring FormatLine(
        const RectangleAreaMeasurement& measurement,
        const CalibrationProfile& calibration,
        MeasurementUnit display_unit);
    static std::wstring FormatLine(
        const PolygonAreaMeasurement& measurement,
        const CalibrationProfile& calibration,
        MeasurementUnit display_unit);
    static std::vector<std::wstring> FormatCollection(
        const MeasurementCollection& measurements,
        const CalibrationProfile& calibration,
        MeasurementUnit display_unit);

private:
    static std::wstring FormatDouble(double value, int precision);
    static int ValuePrecision(const MeasurementResult& result);
};
