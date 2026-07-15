#include "CameraPanelActions.h"

#include "../camera/CameraControlStatusFormatter.h"
#include "../platform/TextInputParser.h"

#include <algorithm>
#include <cstddef>
#include <optional>

CameraListRefreshActionResult CameraPanelActions::SdkUnavailable(const std::wstring& sdk_error)
{
    CameraListRefreshActionResult result;
    result.presentation = CameraDeviceListFormatter::SdkUnavailable();
    result.preview_telemetry = L"SDK not loaded";
    result.status = sdk_error;
    return result;
}

CameraListRefreshActionResult CameraPanelActions::DevicesEnumerated(
    const std::vector<CameraDevice>& devices,
    const std::wstring& sdk_telemetry)
{
    CameraListRefreshActionResult result;
    result.preview_telemetry = sdk_telemetry;
    if (devices.empty()) {
        result.presentation = CameraDeviceListFormatter::NoCameraFound();
        result.status = CameraControlStatusFormatter::FormatNoCameraFound();
        return result;
    }

    result.presentation = CameraDeviceListFormatter::Devices(devices);
    result.camera_count = static_cast<int>(devices.size());
    result.selected_camera_index = result.presentation.default_device_index;
    result.open_enabled = true;
    result.combo_enabled = true;
    result.succeeded = true;
    result.status = CameraControlStatusFormatter::FormatCamerasFound(devices.size());
    return result;
}

CameraSelectionActionResult CameraPanelActions::SelectDevice(int camera_count, int combo_selection)
{
    CameraSelectionActionResult result;
    if (camera_count <= 0) {
        return result;
    }

    const std::optional<int> selected_device_index =
        CameraDeviceListFormatter::SelectionToDeviceIndex(
            combo_selection,
            static_cast<std::size_t>(camera_count));
    result.has_status = true;
    if (!selected_device_index) {
        result.status = CameraControlStatusFormatter::FormatNoCameraSelected();
        return result;
    }

    result.selected_camera_index = *selected_device_index;
    result.status = CameraControlStatusFormatter::FormatSelectedDevice(result.selected_camera_index);
    return result;
}

CameraExposureParseResult CameraPanelActions::ParseExposureText(const std::wstring& text)
{
    CameraExposureParseResult result;
    if (!TextInputParser::TryParsePositiveFloat(text, result.requested_exposure_ms)) {
        result.status = CameraControlStatusFormatter::FormatExposureInvalid();
        return result;
    }

    result.valid = true;
    return result;
}

float CameraPanelActions::ClampExposure(
    float requested_exposure_ms,
    bool has_range,
    float min_value,
    float max_value)
{
    if (!has_range) {
        return requested_exposure_ms;
    }
    return std::clamp(requested_exposure_ms, min_value, max_value);
}

std::wstring CameraPanelActions::ExposureApplyStatus(bool succeeded, float exposure_ms)
{
    if (!succeeded) {
        return CameraControlStatusFormatter::FormatExposureFailed();
    }
    return CameraControlStatusFormatter::FormatExposureSet(exposure_ms);
}

std::wstring CameraPanelActions::ExposurePendingStatus()
{
    return CameraControlStatusFormatter::FormatExposurePending();
}
