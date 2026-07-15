#include "MUCamApi.h"

#include <cwchar>
#include <vector>

namespace {

std::wstring GetExecutableDirectory()
{
    wchar_t path[MAX_PATH] = {};
    const DWORD size = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (size == 0 || size == MAX_PATH) {
        return L".";
    }

    std::wstring result(path, size);
    const std::wstring::size_type slash = result.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return L".";
    }
    return result.substr(0, slash);
}

bool AppendUnique(std::vector<std::wstring>& values, const std::wstring& value)
{
    for (const auto& existing : values) {
        if (_wcsicmp(existing.c_str(), value.c_str()) == 0) {
            return false;
        }
    }
    values.push_back(value);
    return true;
}

} // namespace

MUCamApi::~MUCamApi()
{
    Unload();
}

bool MUCamApi::Load()
{
    if (module_) {
        return true;
    }

    const std::wstring exe_dir = GetExecutableDirectory();
    std::vector<std::wstring> candidates;
#if defined(_WIN64)
    const wchar_t* platform_dir = L"x64";
#else
    const wchar_t* platform_dir = L"x86";
#endif
    AppendUnique(candidates, exe_dir + L"\\MUCam32Ex.dll");
    AppendUnique(candidates, exe_dir + L"\\MUCam32.dll");
    AppendUnique(candidates, L"MUCam32Ex.dll");
    AppendUnique(candidates, L"MUCam32.dll");
    AppendUnique(candidates, std::wstring(L"MUCamSDK\\bin\\") + platform_dir + L"\\MUCam32Ex.dll");
    AppendUnique(candidates, std::wstring(L"MUCamSDK\\lib\\") + platform_dir + L"\\MUCam32Ex.dll");
    AppendUnique(candidates, std::wstring(L"MUCamSDK\\bin\\") + platform_dir + L"\\MUCam32.dll");
    AppendUnique(candidates, std::wstring(L"MUCamSDK\\lib\\") + platform_dir + L"\\MUCam32.dll");

    DWORD last_error = ERROR_MOD_NOT_FOUND;
    for (const auto& candidate : candidates) {
        module_ = LoadLibraryExW(candidate.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (module_) {
            loaded_path_ = candidate;
            last_error_.clear();
            break;
        }
        last_error = GetLastError();
    }

    if (!module_) {
        last_error_ = L"Could not load MUCam SDK DLL. Copy MUCam32Ex.dll and MUCam32.dll next to CameraView.exe. Windows error: ";
        last_error_ += FormatWindowsError(last_error);
        return false;
    }

    if (!ResolveRequired()) {
        Unload();
        return false;
    }

    return true;
}

void MUCamApi::Unload()
{
    find_camera_ = nullptr;
    release_camera_ = nullptr;
    get_type_ = nullptr;
    open_camera_ = nullptr;
    close_camera_ = nullptr;
    is_connected_ = nullptr;
    get_frame_format_ = nullptr;
    get_frame_legacy_ = nullptr;
    get_frame_ex_ = nullptr;
    set_bit_count_ = nullptr;
    get_binning_count_ = nullptr;
    get_binning_list_ = nullptr;
    set_binning_index_ = nullptr;
    set_trigger_type_ = nullptr;
    get_exposure_range_ = nullptr;
    set_exposure_ = nullptr;
    auto_exposure_once_ = nullptr;
    set_auto_exposure_ = nullptr;
    set_rgb_gain_value_ = nullptr;
    calc_white_balance_ = nullptr;
    set_white_balance_ = nullptr;
    get_bayer_format_ = nullptr;
    get_bayer_ = nullptr;
    bayer_to_rgb_ = nullptr;

    if (module_) {
        FreeLibrary(module_);
        module_ = nullptr;
    }
    uses_ex_api_ = false;
}

void MUCamApi::ReleaseCamera(Handle camera) const
{
    if (release_camera_ && camera) {
        release_camera_(camera);
    }
}

void MUCamApi::CloseCamera(Handle camera) const
{
    if (close_camera_ && camera) {
        close_camera_(camera);
    }
}

bool MUCamApi::GetFrame(Handle camera, unsigned char* buffer, int width, int height, unsigned long* timestamp) const
{
    if (!camera || !buffer) {
        return false;
    }
    if (get_frame_ex_) {
        if (timestamp) {
            *timestamp = 0;
        }
        return get_frame_ex_(camera, buffer, width, height);
    }
    return get_frame_legacy_ && get_frame_legacy_(camera, buffer, timestamp);
}

bool MUCamApi::SetBitCount(Handle camera, int bit_count) const
{
    return set_bit_count_ && camera && set_bit_count_(camera, bit_count);
}

bool MUCamApi::GetBinningList(Handle camera, int* widths, int* heights) const
{
    return get_binning_list_ && camera && widths && heights && get_binning_list_(camera, widths, heights);
}

bool MUCamApi::SetBinningIndex(Handle camera, int index) const
{
    return set_binning_index_ && camera && set_binning_index_(camera, index);
}

bool MUCamApi::SetTriggerType(Handle camera, int trigger_type) const
{
    return set_trigger_type_ && camera && set_trigger_type_(camera, trigger_type);
}

bool MUCamApi::GetExposureRange(Handle camera, float* min_value, float* max_value) const
{
    return get_exposure_range_ && camera && min_value && max_value && get_exposure_range_(camera, min_value, max_value);
}

bool MUCamApi::SetExposure(Handle camera, float value) const
{
    return set_exposure_ && camera && set_exposure_(camera, value);
}

bool MUCamApi::ApplyAutoExposure(Handle camera) const
{
    if (!camera) {
        return false;
    }
    if (auto_exposure_once_) {
        constexpr long target_brightness = 128;
        return auto_exposure_once_(camera, target_brightness) > 0.0f;
    }
    if (set_auto_exposure_) {
        constexpr int target_brightness = 128;
        float exposure = 0.0f;
        return set_auto_exposure_(camera, target_brightness, &exposure);
    }
    return false;
}

bool MUCamApi::SetRgbGainValue(Handle camera, float r, float g, float b, int* ri, int* gi, int* bi) const
{
    return set_rgb_gain_value_ && camera && set_rgb_gain_value_(camera, r, g, b, ri, gi, bi);
}

bool MUCamApi::ApplyWhiteBalance(Handle camera) const
{
    if (!camera) {
        return false;
    }
    if (calc_white_balance_) {
        return calc_white_balance_(camera);
    }
    return set_white_balance_ && set_white_balance_(camera, 1);
}

bool MUCamApi::GetBayer(Handle camera, unsigned char* buffer, unsigned long* timestamp) const
{
    return get_bayer_ && camera && buffer && get_bayer_(camera, buffer, timestamp);
}

bool MUCamApi::BayerToRgb(Handle camera, unsigned char* bayer, int format, int width, int height, int bit_count, unsigned char* rgb) const
{
    return bayer_to_rgb_ && camera && bayer && rgb && bayer_to_rgb_(camera, bayer, format, width, height, bit_count, rgb);
}

bool MUCamApi::ResolveRequired()
{
    uses_ex_api_ = ResolveProc("MUCamEx_findCamera", false) != nullptr;

    find_camera_ = reinterpret_cast<FindCameraFn>(ResolveAny("MUCamEx_findCamera", "MUCam_findCamera", true));
    release_camera_ = reinterpret_cast<ReleaseCameraFn>(ResolveAny("MUCamEx_releaseCamera", "MUCam_releaseCamera", true));
    get_type_ = reinterpret_cast<GetTypeFn>(ResolveAny("MUCamEx_getType", "MUCam_getType", true));
    open_camera_ = reinterpret_cast<OpenCameraFn>(ResolveAny("MUCamEx_openCamera", "MUCam_openCamera", true));
    close_camera_ = reinterpret_cast<CloseCameraFn>(ResolveAny("MUCamEx_closeCamera", "MUCam_closeCamera", true));
    is_connected_ = reinterpret_cast<IsConnectedFn>(ResolveAny("MUCamEx_isConnected", "MUCam_isConnected", true));
    get_frame_format_ = reinterpret_cast<GetFrameFormatFn>(ResolveAny("MUCamEx_getFrameFormat", "MUCam_getFrameFormat", true));
    if (uses_ex_api_) {
        get_frame_ex_ = reinterpret_cast<GetFrameExFn>(ResolveProc("MUCamEx_getFrame", true));
    } else {
        get_frame_legacy_ = reinterpret_cast<GetFrameLegacyFn>(ResolveProc("MUCam_getFrame", true));
    }
    set_bit_count_ = reinterpret_cast<SetBitCountFn>(ResolveProc("MUCam_setBitCount", false));
    get_binning_count_ = reinterpret_cast<GetBinningCountFn>(ResolveAny("MUCamEx_getBinningCount", "MUCam_getBinningCount", true));
    get_binning_list_ = reinterpret_cast<GetBinningListFn>(ResolveAny("MUCamEx_getBinningList", "MUCam_getBinningList", true));
    set_binning_index_ = reinterpret_cast<SetBinningIndexFn>(ResolveAny("MUCamEx_setBinningIndex", "MUCam_setBinningIndex", true));

    set_trigger_type_ = reinterpret_cast<SetTriggerTypeFn>(ResolveAny("MUCamEx_setTriggerType", "MUCam_setTriggerType", false));
    get_exposure_range_ = reinterpret_cast<GetExposureRangeFn>(ResolveAny("MUCamEx_getExposureRange", "MUCam_getExposureRange", false));
    set_exposure_ = reinterpret_cast<SetExposureFn>(ResolveAny("MUCamEx_setExposure", "MUCam_setExposure", false));
    auto_exposure_once_ = reinterpret_cast<AutoExposureOnceFn>(ResolveProc("MUCamEx_AutoExposureOnce", false));
    set_auto_exposure_ = reinterpret_cast<SetAutoExposureFn>(ResolveProc("MUCam_setAutoExposure", false));
    set_rgb_gain_value_ = reinterpret_cast<SetRgbGainValueFn>(ResolveAny("MUCamEx_setRGBGainValue", "MUCam_setRGBGainValue", false));
    calc_white_balance_ = reinterpret_cast<CalcWhiteBalanceFn>(ResolveProc("MUCamEx_CalcWhiteBalance", false));
    set_white_balance_ = reinterpret_cast<SetWhiteBalanceFn>(ResolveProc("MUCam_setWhiteBalance", false));
    get_bayer_format_ = reinterpret_cast<GetBayerFormatFn>(ResolveAny("MUCamEx_getBayerFormat", "MUCam_getBayerFormat", false));
    get_bayer_ = reinterpret_cast<GetBayerFn>(ResolveAny("MUCamEx_getBayer", "MUCam_getBayer", false));
    bayer_to_rgb_ = reinterpret_cast<BayerToRgbFn>(ResolveAny("MUCamEx_bayer2RGB", "MUCam_bayer2RGB", false));

    return find_camera_ &&
           release_camera_ &&
           get_type_ &&
           open_camera_ &&
           close_camera_ &&
           is_connected_ &&
           get_frame_format_ &&
           (get_frame_ex_ || get_frame_legacy_) &&
           get_binning_count_ &&
           get_binning_list_ &&
           set_binning_index_;
}

FARPROC MUCamApi::ResolveAny(const char* ex_name, const char* legacy_name, bool required)
{
    FARPROC proc = ResolveProc(uses_ex_api_ ? ex_name : legacy_name, false);
    if (!proc) {
        proc = ResolveProc(uses_ex_api_ ? legacy_name : ex_name, false);
    }
    if (!proc && required) {
        last_error_ = L"Missing required SDK export: ";
        last_error_ += Widen(uses_ex_api_ ? ex_name : legacy_name);
        last_error_ += L". Check that the DLL matches the MUCam SDK in the API document.";
    }
    return proc;
}

FARPROC MUCamApi::ResolveProc(const char* name, bool required)
{
    FARPROC proc = module_ ? GetProcAddress(module_, name) : nullptr;
    if (!proc && required) {
        last_error_ = L"Missing required SDK export: ";
        last_error_ += Widen(name);
        last_error_ += L". Check that the DLL matches the MUCam SDK in the API document.";
    }
    return proc;
}

std::wstring MUCamApi::FormatWindowsError(DWORD error_code)
{
    wchar_t* buffer = nullptr;
    const DWORD chars = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr);

    std::wstring message;
    if (chars && buffer) {
        message.assign(buffer, chars);
        while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' || message.back() == L'.')) {
            message.pop_back();
        }
        LocalFree(buffer);
    } else {
        message = L"Unknown error";
    }
    return message;
}

std::wstring MUCamApi::Widen(const char* text)
{
    std::wstring result;
    if (!text) {
        return result;
    }
    while (*text) {
        result.push_back(static_cast<unsigned char>(*text));
        ++text;
    }
    return result;
}
