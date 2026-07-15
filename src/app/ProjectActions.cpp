#include "ProjectActions.h"

#include "../storage/ProjectRepository.h"

#include <utility>

ProjectActionResult ProjectActions::SaveProject(
    const std::filesystem::path& path,
    const CalibrationProfile& calibration,
    const MeasurementCollection& measurements,
    const std::vector<DyeProfile>& dye_profiles,
    const std::vector<FluorescenceChannel>& fluorescence_channels,
    const EdfOptions& edf_options,
    int stitch_search_percent)
{
    const ProjectDocument document = ProjectSessionMapper::ToDocument(
        calibration,
        measurements,
        dye_profiles,
        fluorescence_channels,
        edf_options,
        stitch_search_percent);

    std::wstring error;
    if (!ProjectRepository::Save(path, document, error)) {
        return {
            false,
            ProjectActionStatus::SaveFailed,
            error.empty() ? L"Failed to save project." : error,
            ProjectSessionState{}};
    }

    return {true, ProjectActionStatus::Saved, L"Project saved.", ProjectSessionState{}};
}

ProjectActionResult ProjectActions::LoadProject(const std::filesystem::path& path)
{
    ProjectDocument document;
    std::wstring error;
    if (!ProjectRepository::Load(path, document, error)) {
        return {
            false,
            ProjectActionStatus::LoadFailed,
            error.empty() ? L"Failed to open project." : error,
            ProjectSessionState{}};
    }

    return {
        true,
        ProjectActionStatus::Loaded,
        L"Project loaded.",
        ProjectSessionMapper::FromDocument(std::move(document))};
}
