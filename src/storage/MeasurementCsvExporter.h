#pragma once

#include "../domain/CalibrationProfile.h"
#include "../domain/MeasurementCollection.h"

#include <filesystem>
#include <string>

class MeasurementCsvExporter {
public:
    static bool Save(
        const std::filesystem::path& path,
        const MeasurementCollection& measurements,
        const CalibrationProfile& calibration,
        MeasurementUnit display_unit,
        std::wstring& error);
};
