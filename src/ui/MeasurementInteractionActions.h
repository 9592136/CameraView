#pragma once

#include "MeasurementActionApplier.h"
#include "MeasurementInteractionState.h"

#include "../domain/CalibrationProfile.h"
#include "../domain/Geometry.h"
#include "../domain/MeasurementCollection.h"

#include <optional>
#include <string>

enum class MeasurementInteractionActionStatus {
    Ignored,
    OutsidePreview,
    NoFrame,
    NoImagePoint,
    Applied
};

struct MeasurementInteractionActionResult {
    bool handled = false;
    bool measurement_list_changed = false;
    bool preview_changed = false;
    MeasurementInteractionActionStatus status = MeasurementInteractionActionStatus::Ignored;
    std::wstring message;
    bool calibration_changed = false;
};

class MeasurementInteractionActions {
public:
    static MeasurementInteractionActionResult AddPoint(
        MeasurementInteractionState& interaction,
        MeasurementCollection& measurements,
        CalibrationProfile& calibration,
        bool point_in_preview,
        bool frame_available,
        std::optional<ImagePoint> image_point,
        double pending_calibration_length,
        MeasurementUnit pending_calibration_unit,
        MeasurementUnit display_unit);

    static MeasurementInteractionActionResult FinishPolygon(
        MeasurementInteractionState& interaction,
        MeasurementCollection& measurements,
        CalibrationProfile& calibration,
        double pending_calibration_length,
        MeasurementUnit pending_calibration_unit,
        MeasurementUnit display_unit);
};
