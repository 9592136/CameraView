#include "CameraDeviceListFormatter.h"

namespace {

CameraDeviceListPresentation SingleDisabledItem(const std::wstring& text)
{
    CameraDeviceListPresentation presentation;
    presentation.items.push_back(text);
    presentation.selected_item = 0;
    return presentation;
}

} // namespace

CameraDeviceListPresentation CameraDeviceListFormatter::SdkUnavailable()
{
    return SingleDisabledItem(L"SDK DLL not loaded");
}

CameraDeviceListPresentation CameraDeviceListFormatter::NoCameraFound()
{
    return SingleDisabledItem(L"No camera found");
}

CameraDeviceListPresentation CameraDeviceListFormatter::Devices(const std::vector<CameraDevice>& devices)
{
    if (devices.empty()) {
        return NoCameraFound();
    }

    CameraDeviceListPresentation presentation;
    presentation.selection_enabled = true;
    presentation.selected_item = 0;
    presentation.default_device_index = 0;
    presentation.items.reserve(devices.size());
    for (const CameraDevice& device : devices) {
        presentation.items.push_back(device.display_name.empty()
            ? L"Device " + std::to_wstring(device.index + 1)
            : device.display_name);
    }
    return presentation;
}

std::optional<int> CameraDeviceListFormatter::SelectionToDeviceIndex(
    int selection,
    std::size_t device_count)
{
    if (selection < 0 || static_cast<std::size_t>(selection) >= device_count) {
        return std::nullopt;
    }
    return selection;
}
