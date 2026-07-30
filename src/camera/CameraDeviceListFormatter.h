#pragma once

#include "CameraDevice.h"
#include "i18n/Localization.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

struct CameraDeviceListPresentation {
    std::vector<std::wstring> items;
    bool selection_enabled = false;
    int selected_item = -1;
    int default_device_index = -1;
};

class CameraDeviceListFormatter {
public:
    static CameraDeviceListPresentation SdkUnavailable(UILanguage lang = UILanguage::English);
    static CameraDeviceListPresentation NoCameraFound(UILanguage lang = UILanguage::English);
    static CameraDeviceListPresentation Devices(const std::vector<CameraDevice>& devices,
                                                 UILanguage lang = UILanguage::English);
    static std::optional<int> SelectionToDeviceIndex(int selection, std::size_t device_count);
};
