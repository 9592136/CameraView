#pragma once

#include "../domain/CalibrationProfile.h"
#include "../domain/Measurement.h"
#include "../imaging/Fluorescence.h"

#include <vector>

struct ProjectProcessingSettings {
    int edf_focus_radius = 1;
    int stitch_search_percent = 50;
};

struct ProjectDocument {
    CalibrationProfile calibration = CalibrationProfile::Uncalibrated();
    std::vector<LengthMeasurement> measurements;
    std::vector<AngleMeasurement> angle_measurements;
    std::vector<RectangleAreaMeasurement> rectangle_measurements;
    std::vector<PolygonAreaMeasurement> polygon_measurements;
    std::vector<DyeProfile> dye_profiles;
    std::vector<FluorescenceChannelRecipe> fluorescence_channels;
    ProjectProcessingSettings processing_settings;
};
