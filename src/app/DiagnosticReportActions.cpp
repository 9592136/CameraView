#include "DiagnosticReportActions.h"

#include <utility>

std::wstring DiagnosticReportActions::BuildReport(
    DiagnosticReportActionInput input,
    const MeasurementCollection& measurements)
{
    DiagnosticReportInput report;
    report.generated = input.generated;
    report.status = std::move(input.status);
    report.preview_telemetry = std::move(input.preview_telemetry);
    report.viewport_zoom = std::move(input.viewport_zoom);
    report.preview_running = input.preview_running;
    report.processing_running = input.processing_running;
    report.sdk = std::move(input.sdk);
    report.sdk_telemetry = DiagnosticReportBuilder::BuildSdkTelemetry(report.sdk);
    report.enumerated_cameras = input.enumerated_cameras;
    report.selected_camera_index = input.selected_camera_index;
    report.enumerated_devices = std::move(input.enumerated_devices);
    report.latest_frame_source = std::move(input.latest_frame_source);
    report.latest_frame = std::move(input.latest_frame);

    report.measurement.calibrated = input.calibration.IsCalibrated();
    report.measurement.microns_per_pixel = input.calibration.MicronsPerPixel();
    report.measurement.display_unit = input.display_unit;
    report.measurement.total_measurements = measurements.Count();
    report.measurement.length_measurements = measurements.LengthCount();
    report.measurement.angle_measurements = measurements.AngleCount();
    report.measurement.rectangle_area_measurements = measurements.RectangleCount();
    report.measurement.polygon_area_measurements = measurements.PolygonCount();

    report.image_processing.pseudo_color = input.pseudo_color;
    report.image_processing.preview_display_mode = std::move(input.preview_display_mode);
    report.image_processing.dye_profiles = input.dye_profiles;
    report.image_processing.fluorescence_channels = input.fluorescence_channels;
    report.image_processing.stitch_tiles = input.stitch_tiles;
    report.image_processing.stitch_search_percent = input.stitch_search_percent;
    report.image_processing.edf_frames = input.edf_frames;
    report.image_processing.edf_focus_radius = input.edf_focus_radius;
    report.image_processing.processing_result_visible = input.processing_result_visible;
    report.image_processing.processing_result_kind = std::move(input.processing_result_kind);
    report.image_processing.processing_result_source = std::move(input.processing_result_source);
    report.image_processing.processing_result_width = input.processing_result_width;
    report.image_processing.processing_result_height = input.processing_result_height;
    report.image_processing.edf_composite_available = input.edf_composite_available;
    report.image_processing.edf_focus_map_available = input.edf_focus_map_available;

    return DiagnosticReportBuilder::Build(report);
}

std::wstring DiagnosticReportActions::BuildSdkTelemetry(const CameraSdkDiagnostics& diagnostics)
{
    return DiagnosticReportBuilder::BuildSdkTelemetry(diagnostics);
}
