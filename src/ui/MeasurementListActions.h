#pragma once

#include "../domain/MeasurementCollection.h"

#include <optional>
#include <string>

enum class MeasurementListActionStatus {
    NoMeasurement,
    NoSelection,
    EmptyName,
    Deleted,
    Renamed
};

struct MeasurementListActionResult {
    MeasurementListActionStatus status = MeasurementListActionStatus::NoSelection;
    bool changed = false;
    std::optional<int> next_selection;
    std::wstring applied_name;
    std::wstring message;
};

class MeasurementListActions {
public:
    static MeasurementListActionResult DeleteSelected(
        MeasurementCollection& measurements,
        int selection);

    static MeasurementListActionResult RenameSelected(
        MeasurementCollection& measurements,
        int selection,
        const std::wstring& requested_name);
};
