#include "MeasurementListSelection.h"

#include <algorithm>

std::optional<std::size_t> MeasurementListSelection::IndexAtSelection(
    int selection,
    std::size_t item_count)
{
    if (selection < 0 || static_cast<std::size_t>(selection) >= item_count) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(selection);
}

std::optional<int> MeasurementListSelection::NextAfterDelete(
    std::size_t deleted_index,
    std::size_t remaining_count)
{
    if (remaining_count == 0) {
        return std::nullopt;
    }

    return static_cast<int>(std::min(deleted_index, remaining_count - 1));
}
