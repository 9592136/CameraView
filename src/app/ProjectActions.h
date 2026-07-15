#pragma once

#include "../domain/CalibrationProfile.h"
#include "../domain/MeasurementCollection.h"
#include "../imaging/EdfProcessor.h"
#include "../imaging/Fluorescence.h"
#include "../storage/ProjectSessionMapper.h"

#include <filesystem>
#include <string>
#include <vector>

enum class ProjectActionStatus {
    Saved,
    Loaded,
    SaveFailed,
    LoadFailed
};

struct ProjectActionResult {
    bool succeeded = false;
    ProjectActionStatus status = ProjectActionStatus::LoadFailed;
    std::wstring message;
    ProjectSessionState session_state;
};

class ProjectActions {
public:
    static ProjectActionResult SaveProject(
        const std::filesystem::path& path,
        const CalibrationProfile& calibration,
        const MeasurementCollection& measurements,
        const std::vector<DyeProfile>& dye_profiles,
        const std::vector<FluorescenceChannel>& fluorescence_channels,
        const EdfOptions& edf_options,
        int stitch_search_percent);

    static ProjectActionResult LoadProject(const std::filesystem::path& path);
};
