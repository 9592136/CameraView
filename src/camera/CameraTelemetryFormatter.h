#pragma once

#include "ICameraDriver.h"
#include "i18n/Localization.h"

#include <string>

class CameraTelemetryFormatter {
public:
    static std::wstring FormatPreviewStarted(int device_index, const CameraOpenInfo& open_info,
                                             UILanguage lang = UILanguage::English);
    static std::wstring FormatPendingTelemetry(int device_index, const CameraOpenInfo& open_info,
                                               UILanguage lang = UILanguage::English);
    static std::wstring FormatFrameTelemetry(
        int device_index, const CameraOpenInfo& open_info,
        double fps, unsigned long timestamp,
        UILanguage lang = UILanguage::English);

private:
    static std::wstring FormatDevicePrefix(int device_index, const CameraOpenInfo& open_info,
                                           UILanguage lang = UILanguage::English);
    static std::wstring FormatResolution(const CameraOpenInfo& open_info);
};
