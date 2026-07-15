#pragma once

#include <cstddef>
#include <optional>

class MeasurementListSelection {
public:
    static std::optional<std::size_t> IndexAtSelection(int selection, std::size_t item_count);
    static std::optional<int> NextAfterDelete(std::size_t deleted_index, std::size_t remaining_count);
};
