#pragma once

#include <cstddef>
#include <string>

class CameraControlStatusFormatter {
public:
    static std::wstring FormatOpeningCamera();
    static std::wstring FormatNoCameraSelectedForStart();
    static std::wstring FormatNoCameraSelected();
    static std::wstring FormatNoCameraFound();
    static std::wstring FormatCamerasFound(std::size_t camera_count);
    static std::wstring FormatSelectedDevice(int device_index);
    static std::wstring FormatSelectedCameraUnavailable();
    static std::wstring FormatFailedToOpenCamera();
    static std::wstring FormatCameraDisconnected();
    static std::wstring FormatPreviewStopped();
    static std::wstring FormatExposureInvalid();
    static std::wstring FormatExposureSet(float exposure_ms);
    static std::wstring FormatExposureFailed();
    static std::wstring FormatExposurePending();

private:
    static std::wstring FormatFloat(float value);
};
