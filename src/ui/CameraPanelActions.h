#pragma once

#include "../camera/CameraDevice.h"
#include "../camera/CameraDeviceListFormatter.h"

#include <string>
#include <vector>

struct CameraListRefreshActionResult {
    CameraDeviceListPresentation presentation;
    int camera_count = 0;
    int selected_camera_index = -1;
    bool open_enabled = false;
    bool combo_enabled = false;
    bool succeeded = false;
    std::wstring preview_telemetry;
    std::wstring status;
};

struct CameraSelectionActionResult {
    int selected_camera_index = -1;
    bool has_status = false;
    std::wstring status;
};

struct CameraExposureParseResult {
    bool valid = false;
    float requested_exposure_ms = 0.0f;
    std::wstring status;
};

class CameraPanelActions {
public:
    static CameraListRefreshActionResult SdkUnavailable(const std::wstring& sdk_error);
    static CameraListRefreshActionResult DevicesEnumerated(
        const std::vector<CameraDevice>& devices,
        const std::wstring& sdk_telemetry);
    static CameraSelectionActionResult SelectDevice(int camera_count, int combo_selection);

    static CameraExposureParseResult ParseExposureText(const std::wstring& text);
    static float ClampExposure(float requested_exposure_ms, bool has_range, float min_value, float max_value);
    static std::wstring ExposureApplyStatus(bool succeeded, float exposure_ms);
    static std::wstring ExposurePendingStatus();
};
