#include "CameraControlStatusFormatter.h"
#include "i18n/Localization.h"

#include <string>

std::wstring CameraControlStatusFormatter::FormatNoCamera()
{
    return L"No camera stream active.";
}

std::wstring CameraControlStatusFormatter::FormatNoCameraSelected(UILanguage lang)
{
    return GetLocStr(LocId::STATUS_NO_CAMERA_SELECTED, lang);
}

std::wstring CameraControlStatusFormatter::FormatNoCameraSelectedShort(UILanguage lang)
{
    return GetLocStr(LocId::STATUS_NO_CAMERA_SELECTED_SHORT, lang);
}

std::wstring CameraControlStatusFormatter::FormatNoMucam()
{
    return L"No MUCam camera found.";
}

std::wstring CameraControlStatusFormatter::FormatMultipleDevicesFound(int count)
{
    return L"Found " + std::to_wstring(count) + L" camera(s). Select a device and click Open.";
}

std::wstring CameraControlStatusFormatter::FormatDeviceSelected(int device_index)
{
    return L"Selected device " + std::to_wstring(device_index + 1) + L". Click Open to preview.";
}

std::wstring CameraControlStatusFormatter::FormatDeviceNoLongerAvailable()
{
    return L"Selected camera is no longer available. Refresh the device list.";
}

std::wstring CameraControlStatusFormatter::FormatOpeningCamera(UILanguage lang)
{
    return GetLocStr(LocId::STATUS_OPENING_CAMERA, lang);
}

std::wstring CameraControlStatusFormatter::FormatOpenError(UILanguage lang)
{
    return GetLocStr(LocId::STATUS_FAILED_TO_OPEN_CAMERA, lang);
}

std::wstring CameraControlStatusFormatter::FormatDisconnected(UILanguage lang)
{
    return GetLocStr(LocId::STATUS_CAMERA_DISCONNECTED, lang);
}

std::wstring CameraControlStatusFormatter::FormatPreviewStopped(UILanguage lang)
{
    return GetLocStr(LocId::STATUS_PREVIEW_STOPPED, lang);
}

std::wstring CameraControlStatusFormatter::FormatExposureNeedsPositive(UILanguage lang)
{
    return GetLocStr(LocId::STATUS_EXPOSURE_NEED_POSITIVE, lang);
}

std::wstring CameraControlStatusFormatter::FormatExposureSet(double ms, UILanguage lang)
{
    std::wstring value_str = std::to_wstring(static_cast<long long>(ms));
    if (ms != static_cast<double>(static_cast<long long>(ms))) {
        wchar_t buf[32];
        swprintf(buf, 32, L"%.2f", ms);
        value_str = buf;
    }
    return FormatLocStr(LocId::STATUS_EXPOSURE_SET, lang, {
        {L"{value}", value_str}
    });
}

std::wstring CameraControlStatusFormatter::FormatExposureFailed(UILanguage lang)
{
    return GetLocStr(LocId::STATUS_EXPOSURE_FAILED, lang);
}

std::wstring CameraControlStatusFormatter::FormatExposurePending(UILanguage lang)
{
    return GetLocStr(LocId::STATUS_EXPOSURE_PENDING, lang);
}
