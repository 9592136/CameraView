#include "MeasurementListActions.h"

#include "../platform/TextInputParser.h"
#include "MeasurementListSelection.h"

#include <utility>

MeasurementListActionResult MeasurementListActions::DeleteSelected(
    MeasurementCollection& measurements,
    int selection)
{
    MeasurementListActionResult result;
    const std::size_t total_measurements = measurements.Count();
    if (total_measurements == 0) {
        result.status = MeasurementListActionStatus::NoMeasurement;
        result.message = L"No measurement to delete.";
        return result;
    }

    const std::optional<std::size_t> selected_index =
        MeasurementListSelection::IndexAtSelection(selection, total_measurements);
    if (!selected_index) {
        result.status = MeasurementListActionStatus::NoSelection;
        result.message = L"Select a measurement first.";
        return result;
    }

    measurements.EraseAtFlatIndex(*selected_index);
    result.status = MeasurementListActionStatus::Deleted;
    result.changed = true;
    result.next_selection =
        MeasurementListSelection::NextAfterDelete(*selected_index, measurements.Count());
    result.message = L"Measurement deleted.";
    return result;
}

MeasurementListActionResult MeasurementListActions::RenameSelected(
    MeasurementCollection& measurements,
    int selection,
    const std::wstring& requested_name)
{
    MeasurementListActionResult result;
    const std::optional<std::size_t> selected_index =
        MeasurementListSelection::IndexAtSelection(selection, measurements.Count());
    if (!selected_index) {
        result.status = MeasurementListActionStatus::NoSelection;
        result.message = L"Select a measurement to rename.";
        return result;
    }

    std::wstring name = TextInputParser::Trim(requested_name);
    if (name.empty()) {
        result.status = MeasurementListActionStatus::EmptyName;
        result.message = L"Enter a measurement name.";
        return result;
    }

    const std::optional<MeasurementReference> reference = measurements.AtFlatIndex(*selected_index);
    if (!reference) {
        result.status = MeasurementListActionStatus::NoSelection;
        result.message = L"Select a measurement to rename.";
        return result;
    }

    measurements.SetName(*reference, name);
    result.status = MeasurementListActionStatus::Renamed;
    result.changed = true;
    const std::optional<std::size_t> restored_selection =
        MeasurementListSelection::IndexAtSelection(selection, measurements.Count());
    if (restored_selection) {
        result.next_selection = static_cast<int>(*restored_selection);
    }
    result.applied_name = std::move(name);
    result.message = L"Measurement renamed.";
    return result;
}
