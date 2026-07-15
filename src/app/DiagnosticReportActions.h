#pragma once

#include "../camera/ICameraDriver.h"
#include "../domain/CalibrationProfile.h"
#include "../domain/ImageFrame.h"
#include "../domain/MeasurementCollection.h"
#include "../imaging/PseudoColorMapper.h"
#include "../storage/DiagnosticReportBuilder.h"

#include <cstddef>
#include <string>
#include <vector>

struct DiagnosticReportActionInput {
    DiagnosticReportTimestamp generated;
    std::wstring status;
    std::wstring preview_telemetry;
    std::wstring viewport_zoom;
    bool preview_running = false;
    bool processing_running = false;
    CameraSdkDiagnostics sdk;
    int enumerated_cameras = 0;
    int selected_camera_index = -1;
    std::vector<CameraDevice> enumerated_devices;
    std::wstring latest_frame_source;
    ImageFrame latest_frame;
    CalibrationProfile calibration = CalibrationProfile::Uncalibrated();
    MeasurementUnit display_unit = MeasurementUnit::Pixels;
    std::wstring preview_display_mode;
    PseudoColorPalette pseudo_color = PseudoColorPalette::Original;
    std::size_t dye_profiles = 0;
    std::size_t fluorescence_channels = 0;
    std::size_t stitch_tiles = 0;
    int stitch_search_percent = 0;
    std::size_t edf_frames = 0;
    int edf_focus_radius = 1;
    bool processing_result_visible = false;
    std::wstring processing_result_kind;
    std::wstring processing_result_source;
    int processing_result_width = 0;
    int processing_result_height = 0;
    bool edf_composite_available = false;
    bool edf_focus_map_available = false;
};

class DiagnosticReportActions {
public:
    static std::wstring BuildReport(
        DiagnosticReportActionInput input,
        const MeasurementCollection& measurements);

    static std::wstring BuildSdkTelemetry(const CameraSdkDiagnostics& diagnostics);
};
