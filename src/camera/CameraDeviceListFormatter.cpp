#include "CameraDeviceListFormatter.h"
#include "i18n/Localization.h"

namespace {

CameraDeviceListPresentation SingleDisabledItem(const std::wstring& text)
{
    CameraDeviceListPresentation presentation;
    presentation.items.push_back(text);
    presentation.selected_item = 0;
    return presentation;
}

} // namespace

CameraDeviceListPresentation CameraDeviceListFormatter::SdkUnavailable(UILanguage lang)
{
    return SingleDisabledItem(GetLocStr(LocId::STATUS_SDK_DLL_NOT_LOADED, lang));
}

CameraDeviceListPresentation CameraDeviceListFormatter::NoCameraFound(UILanguage lang)
{
    return SingleDisabledItem(GetLocStr(LocId::STATUS_NO_CAMERA_FOUND, lang));
}

CameraDeviceListPresentation CameraDeviceListFormatter::Devices(const std::vector<CameraDevice>& devices, UILanguage lang)
{
    if (devices.empty()) {
        return NoCameraFound(lang);
    }

    CameraDeviceListPresentation presentation;
    presentation.selection_enabled = true;
    presentation.selected_item = 0;
    presentation.default_device_index = 0;
    presentation.items.reserve(devices.size());
    const wchar_t* device_prefix = GetLocStr(LocId::STATUS_DEVICE_PREFIX, lang);
    for (const CameraDevice& device : devices) {
        presentation.items.push_back(device.display_name.empty()
            ? std::wstring(device_prefix) + L" " + std::to_wstring(device.index + 1)
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
