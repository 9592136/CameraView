#include "CameraTelemetryFormatter.h"

#include <iomanip>
#include <sstream>

std::wstring CameraTelemetryFormatter::FormatPreviewStarted(int device_index, const CameraOpenInfo& open_info)
{
    return L"Previewing device " + std::to_wstring(device_index + 1) +
           L", camera type " + std::to_wstring(open_info.type) +
           L", " + FormatResolution(open_info) + L".";
}

std::wstring CameraTelemetryFormatter::FormatPendingTelemetry(int device_index, const CameraOpenInfo& open_info)
{
    return FormatDevicePrefix(device_index, open_info) + L" | -- fps";
}

std::wstring CameraTelemetryFormatter::FormatFrameTelemetry(
    int device_index,
    const CameraOpenInfo& open_info,
    double fps,
    unsigned long timestamp)
{
    std::wostringstream status;
    status << FormatDevicePrefix(device_index, open_info)
           << L" | "
           << std::fixed << std::setprecision(1) << fps << L" fps"
           << L" | ts " << timestamp;
    return status.str();
}

std::wstring CameraTelemetryFormatter::FormatDevicePrefix(int device_index, const CameraOpenInfo& open_info)
{
    return L"Device " + std::to_wstring(device_index + 1) +
           L" | type " + std::to_wstring(open_info.type) +
           L" | " + FormatResolution(open_info);
}

std::wstring CameraTelemetryFormatter::FormatResolution(const CameraOpenInfo& open_info)
{
    return std::to_wstring(open_info.width) + L"x" + std::to_wstring(open_info.height);
}
