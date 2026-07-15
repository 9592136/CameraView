#pragma once

#include "../domain/CalibrationProfile.h"
#include "../domain/MeasurementCollection.h"
#include "../imaging/OverlayRenderer.h"
#include "MeasurementInteractionState.h"

class MeasurementOverlayModelBuilder {
public:
    static MeasurementOverlayModel Build(
        const MeasurementCollection& measurements,
        const CalibrationProfile& calibration,
        MeasurementUnit display_unit,
        const MeasurementInteractionPending& pending);

private:
    static MeasurementOverlayPending ToOverlayPending(const MeasurementInteractionPending& pending);
};
