#include "MeasurementInteractionActions.h"

#include <cmath>
#include <utility>

namespace {

constexpr double kMinimumCalibrationDistancePixels = 1.0;

MeasurementInteractionActionResult FromApplyResult(
    MeasurementInteractionActionStatus status,
    MeasurementActionApplyResult apply_result)
{
    return {
        true,
        apply_result.measurement_list_changed,
        apply_result.preview_changed,
        status,
        std::move(apply_result.status)};
}

bool IsCalibrationSecondPointTooClose(
    const MeasurementInteractionState& interaction,
    const std::optional<ImagePoint>& image_point)
{
    if (interaction.Mode() != MeasurementInteractionMode::CalibrationSecondPoint || !image_point) {
        return false;
    }

    const MeasurementInteractionPending pending = interaction.PendingOverlay();
    return DistancePixels(pending.first, *image_point) < kMinimumCalibrationDistancePixels;
}

} // namespace

MeasurementInteractionActionResult MeasurementInteractionActions::AddPoint(
    MeasurementInteractionState& interaction,
    MeasurementCollection& measurements,
    CalibrationProfile& calibration,
    bool point_in_preview,
    bool frame_available,
    std::optional<ImagePoint> image_point,
    double pending_calibration_length,
    MeasurementUnit pending_calibration_unit,
    MeasurementUnit display_unit)
{
    if (interaction.IsIdle()) {
        return {};
    }

    if (!point_in_preview) {
        return {false, false, false, MeasurementInteractionActionStatus::OutsidePreview, L""};
    }

    if (!frame_available) {
        return {true, false, false, MeasurementInteractionActionStatus::NoFrame, L"No frame available."};
    }

    if (!image_point) {
        return {
            true,
            false,
            false,
            MeasurementInteractionActionStatus::NoImagePoint,
            L"Click inside the image area."};
    }

    if (IsCalibrationSecondPointTooClose(interaction, image_point)) {
        return {
            true,
            false,
            true,
            MeasurementInteractionActionStatus::Applied,
            L"Calibration: click a second point farther away."};
    }

    MeasurementInteractionAction action = interaction.AddPoint(*image_point);
    MeasurementInteractionActionResult result = FromApplyResult(
        MeasurementInteractionActionStatus::Applied,
        MeasurementActionApplier::Apply(
            std::move(action),
            measurements,
            calibration,
            pending_calibration_length,
            pending_calibration_unit,
            display_unit));
    result.preview_changed = true;
    return result;
}

MeasurementInteractionActionResult MeasurementInteractionActions::FinishPolygon(
    MeasurementInteractionState& interaction,
    MeasurementCollection& measurements,
    CalibrationProfile& calibration,
    double pending_calibration_length,
    MeasurementUnit pending_calibration_unit,
    MeasurementUnit display_unit)
{
    MeasurementInteractionAction action = interaction.FinishPolygon();
    return FromApplyResult(
        MeasurementInteractionActionStatus::Applied,
        MeasurementActionApplier::Apply(
            std::move(action),
            measurements,
            calibration,
            pending_calibration_length,
            pending_calibration_unit,
            display_unit));
}
