#include "DiagnosticReportBuilder.h"

#include <iomanip>
#include <sstream>

namespace {

std::wstring CameraDeviceDisplayName(const CameraDevice& device)
{
    if (!device.display_name.empty()) {
        return device.display_name;
    }
    return L"Device " + std::to_wstring(device.index + 1);
}

std::wstring CameraDeviceSummary(const CameraDevice& device)
{
    return CameraDeviceDisplayName(device) + L" | type " + std::to_wstring(device.type);
}

} // namespace

std::wstring DiagnosticReportBuilder::Build(const DiagnosticReportInput& input)
{
    const std::wstring sdk_telemetry =
        input.sdk_telemetry.empty() ? BuildSdkTelemetry(input.sdk) : input.sdk_telemetry;

    std::wostringstream report;
    report << L"CameraView Diagnostic Report\n";
    report << L"Generated: "
           << input.generated.year << L"-" << std::setw(2) << std::setfill(L'0') << input.generated.month
           << L"-" << std::setw(2) << input.generated.day
           << L" " << std::setw(2) << input.generated.hour
           << L":" << std::setw(2) << input.generated.minute
           << L":" << std::setw(2) << input.generated.second
           << std::setfill(L' ') << L"\n\n";

    report << L"[Application]\n";
    report << L"Status: " << input.status << L"\n";
    report << L"Preview telemetry: "
           << (input.preview_telemetry.empty() ? L"(none)" : input.preview_telemetry) << L"\n";
    report << L"Viewport zoom: "
           << (input.viewport_zoom.empty() ? L"(none)" : input.viewport_zoom) << L"\n";
    report << L"Preview running: " << YesNo(input.preview_running) << L"\n";
    report << L"Processing running: " << YesNo(input.processing_running) << L"\n\n";

    report << L"[SDK]\n";
    report << L"Loaded: " << YesNo(input.sdk.loaded) << L"\n";
    report << L"Loaded path: " << (input.sdk.loaded_path.empty() ? L"(none)" : input.sdk.loaded_path) << L"\n";
    report << L"Telemetry: " << sdk_telemetry << L"\n";
    report << L"Last error: " << (input.sdk.last_error.empty() ? L"(none)" : input.sdk.last_error) << L"\n";
    report << L"API: " << (input.sdk.uses_ex_api ? L"Ex" : L"Legacy") << L"\n";
    report << L"Exposure control: " << YesNo(input.sdk.has_exposure_control) << L"\n";
    report << L"Auto exposure: " << YesNo(input.sdk.has_auto_exposure_control) << L"\n";
    report << L"Gain control: " << YesNo(input.sdk.has_gain_control) << L"\n";
    report << L"White balance: " << YesNo(input.sdk.has_white_balance_control) << L"\n";
    report << L"Bayer readout: " << YesNo(input.sdk.has_bayer_readout) << L"\n";
    report << L"Bayer to RGB: " << YesNo(input.sdk.has_bayer_to_rgb) << L"\n";
    report << L"Bit depth control: " << YesNo(input.sdk.has_bit_depth_control) << L"\n\n";

    report << L"[Camera]\n";
    report << L"Enumerated cameras: " << input.enumerated_cameras << L"\n";
    report << L"Selected camera index: ";
    if (input.selected_camera_index >= 0) {
        report << (input.selected_camera_index + 1);
    } else {
        report << L"(none)";
    }
    report << L"\n";
    report << L"Selected camera: ";
    if (input.selected_camera_index >= 0 &&
        static_cast<std::size_t>(input.selected_camera_index) < input.enumerated_devices.size()) {
        report << CameraDeviceSummary(input.enumerated_devices[static_cast<std::size_t>(input.selected_camera_index)]);
    } else {
        report << L"(none)";
    }
    report << L"\n";
    if (input.enumerated_devices.empty()) {
        report << L"Camera devices: (none)\n";
    } else {
        report << L"Camera devices:\n";
        for (const CameraDevice& device : input.enumerated_devices) {
            report << L"  " << (device.index + 1) << L". " << CameraDeviceSummary(device) << L"\n";
        }
    }
    report << L"Latest frame source: "
           << (input.latest_frame_source.empty() ? L"(none)" : input.latest_frame_source) << L"\n";
    report << L"Latest frame valid: " << YesNo(input.latest_frame.IsValid()) << L"\n";
    if (input.latest_frame.IsValid()) {
        report << L"Latest frame size: " << input.latest_frame.width << L"x" << input.latest_frame.height << L"\n";
        report << L"Latest frame stride: " << input.latest_frame.stride << L"\n";
        report << L"Latest frame sequence: " << input.latest_frame.sequence << L"\n";
        report << L"Latest frame timestamp: " << input.latest_frame.timestamp << L"\n";
    }
    report << L"\n";

    report << L"[Measurement]\n";
    report << L"Calibrated: " << YesNo(input.measurement.calibrated) << L"\n";
    report << L"Microns per pixel: " << FormatDouble(input.measurement.microns_per_pixel, 8) << L"\n";
    report << L"Display unit: " << CalibrationProfile::UnitLabel(input.measurement.display_unit) << L"\n";
    report << L"Total measurements: " << input.measurement.total_measurements << L"\n";
    report << L"Length measurements: " << input.measurement.length_measurements << L"\n";
    report << L"Angle measurements: " << input.measurement.angle_measurements << L"\n";
    report << L"Rectangle area measurements: " << input.measurement.rectangle_area_measurements << L"\n";
    report << L"Polygon area measurements: " << input.measurement.polygon_area_measurements << L"\n\n";

    report << L"[Image Processing]\n";
    report << L"Preview display mode: "
           << (input.image_processing.preview_display_mode.empty()
                   ? L"(none)"
                   : input.image_processing.preview_display_mode)
           << L"\n";
    report << L"Pseudo color: " << PseudoColorMapper::Label(input.image_processing.pseudo_color) << L"\n";
    report << L"Dye profiles: " << input.image_processing.dye_profiles << L"\n";
    report << L"Fluorescence channels: " << input.image_processing.fluorescence_channels << L"\n";
    report << L"Stitch tiles: " << input.image_processing.stitch_tiles << L"\n";
    report << L"Stitch search percent: " << input.image_processing.stitch_search_percent << L"\n";
    report << L"EDF frames: " << input.image_processing.edf_frames << L"\n";
    report << L"EDF focus radius: " << input.image_processing.edf_focus_radius << L"\n";
    report << L"Processing result visible: " << YesNo(input.image_processing.processing_result_visible) << L"\n";
    report << L"Processing result kind: "
           << (input.image_processing.processing_result_kind.empty()
                   ? L"(none)"
                   : input.image_processing.processing_result_kind)
           << L"\n";
    report << L"Processing result source: "
           << (input.image_processing.processing_result_source.empty()
                   ? L"(none)"
                   : input.image_processing.processing_result_source)
           << L"\n";
    report << L"Processing result size: ";
    if (input.image_processing.processing_result_width > 0 &&
        input.image_processing.processing_result_height > 0) {
        report << input.image_processing.processing_result_width
               << L"x" << input.image_processing.processing_result_height;
    } else {
        report << L"(none)";
    }
    report << L"\n";
    report << L"EDF composite available: "
           << YesNo(input.image_processing.edf_composite_available) << L"\n";
    report << L"EDF focus map available: "
           << YesNo(input.image_processing.edf_focus_map_available) << L"\n";

    return report.str();
}

std::wstring DiagnosticReportBuilder::BuildSdkTelemetry(const CameraSdkDiagnostics& diagnostics)
{
    if (!diagnostics.loaded) {
        return L"SDK not loaded";
    }

    std::wstring sdk_name = FileNameFromPath(diagnostics.loaded_path);
    if (sdk_name.empty()) {
        sdk_name = L"MUCam SDK";
    }

    std::wstring text = L"SDK " + sdk_name;
    text += diagnostics.uses_ex_api ? L" | Ex" : L" | Legacy";
    text += diagnostics.has_exposure_control ? L" | Exposure" : L" | no Exposure";
    if (diagnostics.has_auto_exposure_control) {
        text += L" | AutoExp";
    }
    if (diagnostics.has_gain_control) {
        text += L" | Gain";
    }
    if (diagnostics.has_white_balance_control) {
        text += L" | WB";
    }
    if (diagnostics.has_bayer_readout) {
        text += L" | Bayer";
    }
    if (diagnostics.has_bayer_to_rgb) {
        text += L" | RGB";
    }
    if (diagnostics.has_bit_depth_control) {
        text += L" | 8-bit";
    }
    return text;
}

std::wstring DiagnosticReportBuilder::FormatDouble(double value, int precision)
{
    std::wostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

std::wstring DiagnosticReportBuilder::FileNameFromPath(const std::wstring& path)
{
    const std::wstring::size_type slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return path;
    }
    return path.substr(slash + 1);
}

const wchar_t* DiagnosticReportBuilder::YesNo(bool value)
{
    return value ? L"Yes" : L"No";
}
