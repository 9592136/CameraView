#pragma once

#include "../domain/MeasurementCollection.h"
#include "../imaging/EdfProcessor.h"
#include "../imaging/Fluorescence.h"
#include "../imaging/ImageStitcher.h"
#include "../imaging/ProcessingResultFrames.h"
#include "../imaging/ProcessingRetryState.h"
#include "../storage/ProjectSessionMapper.h"

#include <string>
#include <vector>

struct ProjectRuntimeState {
    CalibrationProfile& calibration;
    MeasurementCollection& measurements;
    std::vector<DyeProfile>& dye_profiles;
    std::vector<FluorescenceChannel>& fluorescence_channels;
    std::vector<StitchTile>& stitch_tiles;
    std::vector<ImageFrame>& edf_stack;
    EdfOptions& edf_options;
    int& stitch_search_percent;
    bool& show_fusion_preview;
    ProcessingRetryState& processing_retry;
    ProcessingResultFrames& processing_frames;
};

struct ProjectSessionRestoreResult {
    bool restored_channel_settings = false;
    std::wstring status;
};

class ProjectSessionRestorer {
public:
    static ProjectSessionRestoreResult Restore(ProjectRuntimeState runtime, ProjectSessionState state);
};
