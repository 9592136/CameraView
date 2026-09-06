#pragma once

#include "../domain/CalibrationProfile.h"
#include "../domain/Measurement.h"
#include "../imaging/Fluorescence.h"

#include <string>
#include <vector>

struct ProjectProcessingSettings {
    int edf_focus_radius = 1;
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
    std::vector<PointMeasurement> point_measurements;
    std::vector<PolylineMeasurement> polyline_measurements;
    std::vector<CircleMeasurement> circle_measurements;
    std::vector<EllipseMeasurement> ellipse_measurements;
    std::vector<MeasurementOverlayStyle> measurement_styles;
    std::vector<DyeProfile> dye_profiles;
    std::vector<FluorescenceChannelRecipe> fluorescence_channels;
    ProjectProcessingSettings processing_settings;
};
