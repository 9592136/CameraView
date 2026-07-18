#pragma once

#include "../domain/CalibrationProfile.h"
#include "../domain/Measurement.h"
#include "../imaging/Fluorescence.h"

#include <string>
#include <vector>

struct ProjectProcessingSettings {
    int edf_focus_radius = 1;
    int stitch_search_percent = 85;
};

struct ObjectiveCalibrationDocument {
    std::wstring objective;
    CalibrationProfile calibration = CalibrationProfile::Uncalibrated();
};

struct ProjectDocument {
    CalibrationProfile calibration = CalibrationProfile::Uncalibrated();
    std::wstring selected_objective;
    std::vector<ObjectiveCalibrationDocument> objective_calibrations;
    std::vector<LengthMeasurement> measurements;
    std::vector<AngleMeasurement> angle_measurements;
    std::vector<RectangleAreaMeasurement> rectangle_measurements;
    std::vector<PolygonAreaMeasurement> polygon_measurements;
    std::vector<DyeProfile> dye_profiles;
    std::vector<FluorescenceChannelRecipe> fluorescence_channels;
    ProjectProcessingSettings processing_settings;
};
