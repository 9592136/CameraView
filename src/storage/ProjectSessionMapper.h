#pragma once

#include "ProjectDocument.h"
#include "../domain/MeasurementCollection.h"
#include "../imaging/EdfProcessor.h"
#include "../imaging/Fluorescence.h"

#include <vector>

struct ProjectSessionState {
    CalibrationProfile calibration = CalibrationProfile::Uncalibrated();
    MeasurementCollection measurements;
    std::vector<DyeProfile> dye_profiles;
    std::vector<FluorescenceChannel> fluorescence_channels;
    EdfOptions edf_options;
    int stitch_search_percent = 85;
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
        int stitch_search_percent);

    static ProjectSessionState FromDocument(ProjectDocument document);

private:
    static FluorescenceChannelRecipe ToRecipe(const FluorescenceChannel& channel);
    static FluorescenceChannel ToChannel(const FluorescenceChannelRecipe& recipe);
};
