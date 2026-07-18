#include "MeasurementDisplayActions.h"

#include "../domain/MeasurementFormatter.h"
#include "MeasurementListSelection.h"
#include "MeasurementOverlayModelBuilder.h"

#include <iomanip>
#include <sstream>

MeasurementUnit MeasurementDisplayActions::DisplayUnit(const CalibrationProfile& calibration)
{
    return calibration.IsCalibrated() ? MeasurementUnit::Micrometers : MeasurementUnit::Pixels;
}

std::wstring MeasurementDisplayActions::CalibrationStatusLine(const CalibrationProfile& calibration)
{
    return CalibrationStatusLine(L"Current scale", calibration);
}

std::wstring MeasurementDisplayActions::CalibrationStatusLine(
    const std::wstring& objective_label,
    const CalibrationProfile& calibration)
{
    if (!calibration.IsCalibrated()) {
        return objective_label + L": uncalibrated";
    }

    std::wostringstream stream;
    stream << objective_label << L": " << std::fixed << std::setprecision(4)
           << calibration.MicronsPerPixel() << L" um/px";
    return stream.str();
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
