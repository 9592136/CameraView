#include "CameraTelemetryFormatter.h"
#include "i18n/Localization.h"

#include <iomanip>
#include <sstream>

std::wstring CameraTelemetryFormatter::FormatPreviewStarted(int device_index, const CameraOpenInfo& open_info, UILanguage lang)
{
    return FormatLocStr(LocId::STATUS_PREVIEWING_DEVICE, lang, {
        {L"{idx}", std::to_wstring(device_index + 1)},
        {L"{type}", std::to_wstring(open_info.type)},
        {L"{res}", FormatResolution(open_info)}
    });
}

std::wstring CameraTelemetryFormatter::FormatPendingTelemetry(int device_index, const CameraOpenInfo& open_info, UILanguage lang)
{
    return FormatLocStr(LocId::STATUS_PENDING_TELEMETRY, lang, {
        {L"{idx}", std::to_wstring(device_index + 1)},
        {L"{type}", std::to_wstring(open_info.type)},
        {L"{res}", FormatResolution(open_info)}
    });
}

std::wstring CameraTelemetryFormatter::FormatFrameTelemetry(
    int device_index,
    const CameraOpenInfo& open_info,
    double fps,
    unsigned long timestamp,
    UILanguage lang)
{
    std::wostringstream fps_str;
    fps_str << std::fixed << std::setprecision(1) << fps;
    return FormatLocStr(LocId::STATUS_TELEMETRY_FRAME, lang, {
        {L"{idx}", std::to_wstring(device_index + 1)},
        {L"{type}", std::to_wstring(open_info.type)},
        {L"{res}", FormatResolution(open_info)},
        {L"{fps}", fps_str.str()},
        {L"{ts}", std::to_wstring(timestamp)}
    });
}

std::wstring CameraTelemetryFormatter::FormatDevicePrefix(int device_index, const CameraOpenInfo& open_info, UILanguage lang)
{
    return FormatLocStr(LocId::STATUS_DEVICE_INFO, lang, {
        {L"{idx}", std::to_wstring(device_index + 1)},
        {L"{type}", std::to_wstring(open_info.type)},
        {L"{w}", std::to_wstring(open_info.width)},
        {L"{h}", std::to_wstring(open_info.height)}
    });
}

std::wstring CameraTelemetryFormatter::FormatResolution(const CameraOpenInfo& open_info)
{
    return std::to_wstring(open_info.width) + L"x" + std::to_wstring(open_info.height);
}
