#pragma once

#include "CameraDevice.h"

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
    static CameraDeviceListPresentation SdkUnavailable();
    static CameraDeviceListPresentation NoCameraFound();
    static CameraDeviceListPresentation Devices(const std::vector<CameraDevice>& devices);
    static std::optional<int> SelectionToDeviceIndex(int selection, std::size_t device_count);
};
