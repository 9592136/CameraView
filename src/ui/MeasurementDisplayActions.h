#pragma once

#include "../domain/CalibrationProfile.h"
#include "../domain/MeasurementCollection.h"
#include "../imaging/OverlayRenderer.h"
#include "MeasurementInteractionState.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

class MeasurementDisplayActions {
public:
    static MeasurementUnit DisplayUnit(const CalibrationProfile& calibration);
    static std::wstring CalibrationStatusLine(const CalibrationProfile& calibration);
    static std::wstring CalibrationStatusLine(
        const std::wstring& objective_label,
        const CalibrationProfile& calibration);
    static std::vector<std::wstring> ListLines(
        const MeasurementCollection& measurements,
        const CalibrationProfile& calibration);
    static MeasurementOverlayModel BuildOverlayModel(
        const MeasurementCollection& measurements,
        const CalibrationProfile& calibration,
        const MeasurementInteractionState& interaction);
    static std::optional<MeasurementReference> SelectedMeasurement(
        const MeasurementCollection& measurements,
        int selected_index);
    static std::size_t MeasurementCount(const MeasurementCollection& measurements);
};
