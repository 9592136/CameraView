#pragma once

#include "MeasurementCollection.h"

#include <cstddef>
#include <string>

class MeasurementNameFormatter {
public:
    static std::wstring FormatDefaultName(MeasurementKind kind, std::size_t one_based_index);
    static std::wstring NextDefaultName(MeasurementKind kind, const MeasurementCollection& measurements);

private:
    static const wchar_t* PrefixFor(MeasurementKind kind);
    static std::size_t CountFor(MeasurementKind kind, const MeasurementCollection& measurements);
};
