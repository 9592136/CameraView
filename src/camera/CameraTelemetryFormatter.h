#pragma once

#include "ICameraDriver.h"

#include <string>

class CameraTelemetryFormatter {
public:
    static std::wstring FormatPreviewStarted(int device_index, const CameraOpenInfo& open_info);
    static std::wstring FormatPendingTelemetry(int device_index, const CameraOpenInfo& open_info);
    static std::wstring FormatFrameTelemetry(
        int device_index,
        const CameraOpenInfo& open_info,
        double fps,
        unsigned long timestamp);

private:
    static std::wstring FormatDevicePrefix(int device_index, const CameraOpenInfo& open_info);
    static std::wstring FormatResolution(const CameraOpenInfo& open_info);
};
