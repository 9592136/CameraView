#include "MeasurementToolAvailability.h"

MeasurementToolStartResult MeasurementToolAvailability::ForFrame(
    const ImageFrame& frame,
    MeasurementToolStartKind kind)
{
    if (frame.IsValid()) {
        return {true, L""};
    }

    switch (kind) {
    case MeasurementToolStartKind::Calibration:
        return {false, L"Open a camera frame before calibration."};
    case MeasurementToolStartKind::Measurement:
    default:
        return {false, L"Open a camera frame before measuring."};
    }
}

MeasurementToolStartResult MeasurementToolAvailability::ForCalibration(const ImageFrame& frame)
{
    return ForFrame(frame, MeasurementToolStartKind::Calibration);
}

MeasurementToolStartResult MeasurementToolAvailability::ForMeasurement(const ImageFrame& frame)
{
    return ForFrame(frame, MeasurementToolStartKind::Measurement);
}
