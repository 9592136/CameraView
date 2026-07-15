#include "CameraControlStatusFormatter.h"

#include <iomanip>
#include <sstream>

std::wstring CameraControlStatusFormatter::FormatOpeningCamera()
{
    return L"Opening camera...";
}

std::wstring CameraControlStatusFormatter::FormatNoCameraSelectedForStart()
{
    return L"No camera selected. Click Refresh and choose a device.";
}

std::wstring CameraControlStatusFormatter::FormatNoCameraSelected()
{
    return L"No camera selected.";
}

std::wstring CameraControlStatusFormatter::FormatNoCameraFound()
{
    return L"No MUCam camera found.";
}

std::wstring CameraControlStatusFormatter::FormatCamerasFound(std::size_t camera_count)
{
    return L"Found " + std::to_wstring(camera_count) + L" camera(s). Select a device and click Open.";
}

std::wstring CameraControlStatusFormatter::FormatSelectedDevice(int device_index)
{
    return L"Selected device " + std::to_wstring(device_index + 1) + L". Click Open to preview.";
}

std::wstring CameraControlStatusFormatter::FormatSelectedCameraUnavailable()
{
    return L"Selected camera is no longer available. Refresh the device list.";
}

std::wstring CameraControlStatusFormatter::FormatFailedToOpenCamera()
{
    return L"Failed to open camera.";
}

std::wstring CameraControlStatusFormatter::FormatCameraDisconnected()
{
    return L"Camera disconnected.";
}

std::wstring CameraControlStatusFormatter::FormatPreviewStopped()
{
    return L"Preview stopped.";
}

std::wstring CameraControlStatusFormatter::FormatExposureInvalid()
{
    return L"Exposure must be a positive number.";
}

std::wstring CameraControlStatusFormatter::FormatExposureSet(float exposure_ms)
{
    return L"Exposure set to " + FormatFloat(exposure_ms) + L" ms.";
}

std::wstring CameraControlStatusFormatter::FormatExposureFailed()
{
    return L"Failed to set exposure.";
}

std::wstring CameraControlStatusFormatter::FormatExposurePending()
{
    return L"Exposure will be applied when the camera opens.";
}

std::wstring CameraControlStatusFormatter::FormatFloat(float value)
{
    std::wostringstream stream;
    stream << std::fixed << std::setprecision(2) << value;
    return stream.str();
}
