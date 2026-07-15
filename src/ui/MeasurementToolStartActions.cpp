#include "MeasurementToolStartActions.h"

#include <utility>

namespace {

MeasurementToolStartActionResult FrameUnavailableResult(
    MeasurementToolStartActionKind kind,
    const MeasurementToolStartResult& availability)
{
    return {
        false,
        kind,
        MeasurementToolStartActionStatus::FrameUnavailable,
        availability.status};
}

MeasurementToolStartActionResult StartedResult(
    MeasurementToolStartActionKind kind,
    std::wstring message)
{
    return {
        true,
        kind,
        MeasurementToolStartActionStatus::Started,
        std::move(message)};
}

} // namespace

MeasurementToolStartActionResult MeasurementToolStartActions::BeginCalibration(
    MeasurementInteractionState& interaction,
    const MeasurementToolStartResult& availability,
    bool length_valid,
    double length,
    MeasurementUnit unit)
{
    if (!length_valid) {
        return {
            false,
            MeasurementToolStartActionKind::Calibration,
            MeasurementToolStartActionStatus::InvalidCalibrationLength,
            L"Enter a positive calibration length."};
    }

    if (!availability.can_start) {
        return FrameUnavailableResult(MeasurementToolStartActionKind::Calibration, availability);
    }

    MeasurementToolStartActionResult result =
        StartedResult(MeasurementToolStartActionKind::Calibration, interaction.BeginCalibration(length, unit));
    result.calibration_length = length;
    result.calibration_unit = unit;
    return result;
}

MeasurementToolStartActionResult MeasurementToolStartActions::BeginLength(
    MeasurementInteractionState& interaction,
    const MeasurementToolStartResult& availability)
{
    if (!availability.can_start) {
        return FrameUnavailableResult(MeasurementToolStartActionKind::Length, availability);
    }
    return StartedResult(MeasurementToolStartActionKind::Length, interaction.BeginLength());
}

MeasurementToolStartActionResult MeasurementToolStartActions::BeginAngle(
    MeasurementInteractionState& interaction,
    const MeasurementToolStartResult& availability)
{
    if (!availability.can_start) {
        return FrameUnavailableResult(MeasurementToolStartActionKind::Angle, availability);
    }
    return StartedResult(MeasurementToolStartActionKind::Angle, interaction.BeginAngle());
}

MeasurementToolStartActionResult MeasurementToolStartActions::BeginRectangleArea(
    MeasurementInteractionState& interaction,
    const MeasurementToolStartResult& availability)
{
    if (!availability.can_start) {
        return FrameUnavailableResult(MeasurementToolStartActionKind::RectangleArea, availability);
    }
    return StartedResult(MeasurementToolStartActionKind::RectangleArea, interaction.BeginRectangleArea());
}

MeasurementToolStartActionResult MeasurementToolStartActions::BeginPolygonArea(
    MeasurementInteractionState& interaction,
    const MeasurementToolStartResult& availability)
{
    if (!availability.can_start) {
        return FrameUnavailableResult(MeasurementToolStartActionKind::PolygonArea, availability);
    }
    return StartedResult(MeasurementToolStartActionKind::PolygonArea, interaction.BeginPolygonArea());
}
