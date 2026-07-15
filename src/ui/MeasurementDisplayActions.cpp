#include "MeasurementDisplayActions.h"

#include "../domain/MeasurementFormatter.h"
#include "MeasurementListSelection.h"
#include "MeasurementOverlayModelBuilder.h"

MeasurementUnit MeasurementDisplayActions::DisplayUnit(const CalibrationProfile& calibration)
{
    return calibration.IsCalibrated() ? MeasurementUnit::Micrometers : MeasurementUnit::Pixels;
}

std::vector<std::wstring> MeasurementDisplayActions::ListLines(
    const MeasurementCollection& measurements,
    const CalibrationProfile& calibration)
{
    return MeasurementFormatter::FormatCollection(
        measurements,
        calibration,
        DisplayUnit(calibration));
}

MeasurementOverlayModel MeasurementDisplayActions::BuildOverlayModel(
    const MeasurementCollection& measurements,
    const CalibrationProfile& calibration,
    const MeasurementInteractionState& interaction)
{
    return MeasurementOverlayModelBuilder::Build(
        measurements,
        calibration,
        DisplayUnit(calibration),
        interaction.PendingOverlay());
}

std::optional<MeasurementReference> MeasurementDisplayActions::SelectedMeasurement(
    const MeasurementCollection& measurements,
    int selected_index)
{
    const std::optional<std::size_t> index =
        MeasurementListSelection::IndexAtSelection(selected_index, measurements.Count());
    if (!index) {
        return std::nullopt;
    }
    return measurements.AtFlatIndex(*index);
}

std::size_t MeasurementDisplayActions::MeasurementCount(const MeasurementCollection& measurements)
{
    return measurements.Count();
}
