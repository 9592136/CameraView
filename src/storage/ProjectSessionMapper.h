#pragma once

#include "ProjectDocument.h"
#include "../domain/MeasurementCollection.h"
#include "../imaging/EdfProcessor.h"
#include "../imaging/Fluorescence.h"

#include <vector>

struct ProjectSessionState {
    CalibrationProfile calibration = CalibrationProfile::Uncalibrated();
    std::vector<std::wstring> objective_labels;
    std::vector<CalibrationProfile> objective_calibrations;
    int selected_objective_index = 0;
    MeasurementCollection measurements;
    std::vector<DyeProfile> dye_profiles;
    std::vector<FluorescenceChannel> fluorescence_channels;
    EdfOptions edf_options;
    int stitch_search_percent = 85;
    bool stitch_use_orb_registration = true;
    int stitch_overlap_percent = 25;
    int stitch_layout_mode = 0;
    int stitch_grid_rows = 3;
    int stitch_grid_cols = 4;
    int stitch_registration_method = 4;
    int stitch_transform_model = 1;
    int stitch_blend_mode = 0;
    int live_stitch_interval_ms = 1200;
    int fluorescence_blend_mode = 1;
    bool restored_channel_settings = false;
};

class ProjectSessionMapper {
public:
    static ProjectDocument ToDocument(
        const CalibrationProfile& calibration,
        const MeasurementCollection& measurements,
        const std::vector<DyeProfile>& dye_profiles,
        const std::vector<FluorescenceChannel>& fluorescence_channels,
        const EdfOptions& edf_options,
        int stitch_search_percent,
        const std::vector<std::wstring>& objective_labels = std::vector<std::wstring>(),
        const std::vector<CalibrationProfile>& objective_calibrations = std::vector<CalibrationProfile>(),
        int selected_objective_index = 0,
        bool stitch_use_orb_registration = true);

    static ProjectSessionState FromDocument(ProjectDocument document);

private:
    static FluorescenceChannelRecipe ToRecipe(const FluorescenceChannel& channel);
    static FluorescenceChannel ToChannel(const FluorescenceChannelRecipe& recipe);
};
