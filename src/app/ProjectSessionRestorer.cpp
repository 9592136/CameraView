#include "ProjectSessionRestorer.h"

#include <utility>

ProjectSessionRestoreResult ProjectSessionRestorer::Restore(ProjectRuntimeState runtime, ProjectSessionState state)
{
    ProjectSessionRestoreResult result;
    result.restored_channel_settings = state.restored_channel_settings;
    result.status = result.restored_channel_settings
        ? L"Project opened. Fluorescence channel settings restored without image frames."
        : L"Project opened.";

    runtime.calibration = state.calibration;
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
