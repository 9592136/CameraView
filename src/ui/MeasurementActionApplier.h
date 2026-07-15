#pragma once

#include "../domain/CalibrationProfile.h"
#include "../domain/MeasurementCollection.h"
#include "MeasurementInteractionState.h"

#include <string>

struct MeasurementActionApplyResult {
    bool measurement_list_changed = false;
    bool preview_changed = false;
    std::wstring status;
};

class MeasurementActionApplier {
public:
    static MeasurementActionApplyResult Apply(
        MeasurementInteractionAction action,
        MeasurementCollection& measurements,
        CalibrationProfile& calibration,
        double calibration_length,
        MeasurementUnit calibration_unit,
        MeasurementUnit display_unit);

private:
    static std::wstring FormatDouble(double value, int precision);
};
