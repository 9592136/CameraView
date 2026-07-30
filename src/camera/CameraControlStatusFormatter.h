#pragma once

#include "i18n/Localization.h"

#include <string>

class CameraControlStatusFormatter {
public:
    static std::wstring FormatNoCamera();
    static std::wstring FormatNoCameraSelected(UILanguage lang = UILanguage::English);
    static std::wstring FormatNoCameraSelectedShort(UILanguage lang = UILanguage::English);
    static std::wstring FormatNoMucam();
    static std::wstring FormatMultipleDevicesFound(int count);
    static std::wstring FormatDeviceSelected(int device_index);
    static std::wstring FormatDeviceNoLongerAvailable();
    static std::wstring FormatOpeningCamera(UILanguage lang = UILanguage::English);
    static std::wstring FormatOpenError(UILanguage lang = UILanguage::English);
    static std::wstring FormatDisconnected(UILanguage lang = UILanguage::English);
    static std::wstring FormatPreviewStopped(UILanguage lang = UILanguage::English);
    static std::wstring FormatExposureNeedsPositive(UILanguage lang = UILanguage::English);
    static std::wstring FormatExposureSet(double ms, UILanguage lang = UILanguage::English);
    static std::wstring FormatExposureFailed(UILanguage lang = UILanguage::English);
    static std::wstring FormatExposurePending(UILanguage lang = UILanguage::English);
};
