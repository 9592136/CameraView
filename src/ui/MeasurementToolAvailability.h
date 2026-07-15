#pragma once

#include "../domain/ImageFrame.h"

#include <string>

enum class MeasurementToolStartKind {
    Calibration,
    Measurement
};

struct MeasurementToolStartResult {
    bool can_start = false;
    std::wstring status;
};

class MeasurementToolAvailability {
public:
    static MeasurementToolStartResult ForFrame(
        const ImageFrame& frame,
        MeasurementToolStartKind kind);
    static MeasurementToolStartResult ForCalibration(const ImageFrame& frame);
    static MeasurementToolStartResult ForMeasurement(const ImageFrame& frame);
};
