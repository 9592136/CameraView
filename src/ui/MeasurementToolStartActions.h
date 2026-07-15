#pragma once

#include "MeasurementInteractionState.h"
#include "MeasurementToolAvailability.h"

#include "../domain/CalibrationProfile.h"

#include <string>

enum class MeasurementToolStartActionKind {
    Calibration,
    Length,
    Angle,
    RectangleArea,
    PolygonArea
};

enum class MeasurementToolStartActionStatus {
    Started,
    InvalidCalibrationLength,
    FrameUnavailable
};

struct MeasurementToolStartActionResult {
    bool started = false;
    MeasurementToolStartActionKind kind = MeasurementToolStartActionKind::Length;
    MeasurementToolStartActionStatus status = MeasurementToolStartActionStatus::FrameUnavailable;
    std::wstring message;
    double calibration_length = 0.0;
    MeasurementUnit calibration_unit = MeasurementUnit::Micrometers;
};

class MeasurementToolStartActions {
public:
    static MeasurementToolStartActionResult BeginCalibration(
        MeasurementInteractionState& interaction,
        const MeasurementToolStartResult& availability,
        bool length_valid,
        double length,
        MeasurementUnit unit);

    static MeasurementToolStartActionResult BeginLength(
        MeasurementInteractionState& interaction,
        const MeasurementToolStartResult& availability);

    static MeasurementToolStartActionResult BeginAngle(
        MeasurementInteractionState& interaction,
        const MeasurementToolStartResult& availability);

    static MeasurementToolStartActionResult BeginRectangleArea(
        MeasurementInteractionState& interaction,
        const MeasurementToolStartResult& availability);

    static MeasurementToolStartActionResult BeginPolygonArea(
        MeasurementInteractionState& interaction,
        const MeasurementToolStartResult& availability);
};
