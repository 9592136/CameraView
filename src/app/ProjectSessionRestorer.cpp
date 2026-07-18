#include "ProjectSessionRestorer.h"

#include <utility>

namespace {

int NormalizeObjectiveIndex(int index, std::size_t count)
{
    if (count == 0 || index < 0 || index >= static_cast<int>(count)) {
        return 0;
    }
    return index;
}

std::vector<std::wstring> DefaultObjectiveLabels()
{
    return CalibrationProfile::ObjectiveMagnificationOptions();
}

} // namespace

ProjectSessionRestoreResult ProjectSessionRestorer::Restore(ProjectRuntimeState runtime, ProjectSessionState state)
{
    ProjectSessionRestoreResult result;
    result.restored_channel_settings = state.restored_channel_settings;
    result.status = result.restored_channel_settings
        ? L"Project opened. Fluorescence channel settings restored without image frames."
        : L"Project opened.";

    runtime.objective_labels = state.objective_labels.empty()
        ? DefaultObjectiveLabels()
        : std::move(state.objective_labels);
    runtime.selected_objective_index =
        NormalizeObjectiveIndex(state.selected_objective_index, runtime.objective_labels.size());
    runtime.objective_calibrations.assign(
        runtime.objective_labels.size(),
        CalibrationProfile::Uncalibrated());
    for (std::size_t index = 0;
         index < runtime.objective_labels.size() && index < state.objective_calibrations.size();
         ++index) {
        runtime.objective_calibrations[index] = state.objective_calibrations[index];
    }
    runtime.calibration =
        runtime.objective_calibrations[static_cast<std::size_t>(runtime.selected_objective_index)];
    runtime.measurements = std::move(state.measurements);
    runtime.dye_profiles = std::move(state.dye_profiles);
    runtime.fluorescence_channels = std::move(state.fluorescence_channels);
    runtime.edf_options = state.edf_options;
    runtime.stitch_search_percent = state.stitch_search_percent;
    runtime.show_fusion_preview = false;

    runtime.stitch_tiles.clear();
    runtime.edf_stack.clear();
    runtime.processing_retry.Clear();
    runtime.processing_frames.Clear();

    return result;
}
