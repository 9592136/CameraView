#pragma once

#include "../camera/ICameraDriver.h"
#include "../domain/CalibrationProfile.h"
#include "../domain/ImageFrame.h"
#include "../imaging/PseudoColorMapper.h"

#include <cstddef>
#include <string>
#include <vector>

struct DiagnosticReportTimestamp {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
};

struct DiagnosticMeasurementSummary {
    std::wstring objective_label;
    bool calibrated = false;
    double microns_per_pixel = 0.0;
    MeasurementUnit display_unit = MeasurementUnit::Pixels;
    std::size_t total_measurements = 0;
    std::size_t length_measurements = 0;
    std::size_t angle_measurements = 0;
    std::size_t rectangle_area_measurements = 0;
    std::size_t polygon_area_measurements = 0;
};

struct DiagnosticImageProcessingSummary {
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

struct DiagnosticReportInput {
    DiagnosticReportTimestamp generated;
    std::wstring status;
    std::wstring preview_telemetry;
    std::wstring viewport_zoom;
    bool preview_running = false;
    bool processing_running = false;
    CameraSdkDiagnostics sdk;
    std::wstring sdk_telemetry;
    int enumerated_cameras = 0;
    int selected_camera_index = -1;
    std::vector<CameraDevice> enumerated_devices;
    std::wstring latest_frame_source;
    ImageFrame latest_frame;
    DiagnosticMeasurementSummary measurement;
    DiagnosticImageProcessingSummary image_processing;
};

class DiagnosticReportBuilder {
public:
    static std::wstring Build(const DiagnosticReportInput& input);
    static std::wstring BuildFromTemplate(
        const DiagnosticReportInput& input,
        const std::wstring& template_text);
    static std::wstring BuildSdkTelemetry(const CameraSdkDiagnostics& diagnostics);

private:
    static std::wstring FormatTimestamp(const DiagnosticReportTimestamp& timestamp);
    static std::wstring FormatDouble(double value, int precision);
    static std::wstring FileNameFromPath(const std::wstring& path);
    static const wchar_t* YesNo(bool value);
};
