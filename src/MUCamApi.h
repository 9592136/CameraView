#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>

#ifndef MUCAM_CALL
#define MUCAM_CALL __cdecl
#endif

class MUCamApi {
public:
    using Handle = void*;

    enum FrameFormat {
        MUCAM_FORMAT_BAYER_GR_BG = 0,
        MUCAM_FORMAT_BAYER_BG_GR = 1,
        MUCAM_FORMAT_BAYER_GB_RG = 2,
        MUCAM_FORMAT_BAYER_RG_GB = 3,
        MUCAM_FORMAT_COLOR_RGB = 4,
        MUCAM_FORMAT_COLOR_BGR = 5,
        MUCAM_FORMAT_MONOCHROME = 6
    };

    MUCamApi() = default;
    ~MUCamApi();

    MUCamApi(const MUCamApi&) = delete;
    MUCamApi& operator=(const MUCamApi&) = delete;

    bool Load();
    void Unload();

    bool IsLoaded() const { return module_ != nullptr; }
    const std::wstring& LastError() const { return last_error_; }
    const std::wstring& LoadedPath() const { return loaded_path_; }

    bool UsesExApi() const { return uses_ex_api_; }
    bool HasBitDepthControl() const { return set_bit_count_ != nullptr; }

    Handle FindCamera() const { return find_camera_ ? find_camera_() : nullptr; }
    void ReleaseCamera(Handle camera) const;
    int GetType(Handle camera) const { return get_type_ ? get_type_(camera) : -1; }
    bool OpenCamera(Handle camera) const { return open_camera_ ? open_camera_(camera) : false; }
    void CloseCamera(Handle camera) const;
    bool IsConnected(Handle camera) const { return is_connected_ ? is_connected_(camera) : false; }
    int GetFrameFormat(Handle camera) const { return get_frame_format_ ? get_frame_format_(camera) : MUCAM_FORMAT_COLOR_BGR; }
    bool GetFrame(Handle camera, unsigned char* buffer, int width, int height, unsigned long* timestamp) const;
    bool SetBitCount(Handle camera, int bit_count) const;
    int GetBinningCount(Handle camera) const { return get_binning_count_ ? get_binning_count_(camera) : 0; }
    bool GetBinningList(Handle camera, int* widths, int* heights) const;
    bool SetBinningIndex(Handle camera, int index) const;
    bool SetTriggerType(Handle camera, int trigger_type) const;
    bool GetExposureRange(Handle camera, float* min_value, float* max_value) const;
    bool SetExposure(Handle camera, float value) const;
    bool ApplyAutoExposure(Handle camera) const;
    bool SetRgbGainValue(Handle camera, float r, float g, float b, int* ri, int* gi, int* bi) const;
    bool ApplyWhiteBalance(Handle camera) const;
    int GetBayerFormat(Handle camera) const { return get_bayer_format_ ? get_bayer_format_(camera) : -1; }
    bool GetBayer(Handle camera, unsigned char* buffer, unsigned long* timestamp) const;
    bool BayerToRgb(Handle camera, unsigned char* bayer, int format, int width, int height, int bit_count, unsigned char* rgb) const;

    bool HasExposureControl() const { return get_exposure_range_ != nullptr && set_exposure_ != nullptr; }
    bool HasAutoExposureControl() const { return auto_exposure_once_ != nullptr || set_auto_exposure_ != nullptr; }
    bool HasGainControl() const { return set_rgb_gain_value_ != nullptr; }
    bool HasWhiteBalanceControl() const { return calc_white_balance_ != nullptr || set_white_balance_ != nullptr; }
    bool HasBayerReadout() const { return get_bayer_format_ != nullptr && get_bayer_ != nullptr; }
    bool HasBayerToRgb() const { return bayer_to_rgb_ != nullptr; }

private:
    using FindCameraFn = Handle(MUCAM_CALL*)();
    using ReleaseCameraFn = void(MUCAM_CALL*)(Handle);
    using GetTypeFn = int(MUCAM_CALL*)(Handle);
    using OpenCameraFn = bool(MUCAM_CALL*)(Handle);
    using CloseCameraFn = void(MUCAM_CALL*)(Handle);
    using IsConnectedFn = bool(MUCAM_CALL*)(Handle);
    using GetFrameFormatFn = int(MUCAM_CALL*)(Handle);
    using GetFrameLegacyFn = bool(MUCAM_CALL*)(Handle, unsigned char*, unsigned long*);
    using GetFrameExFn = bool(MUCAM_CALL*)(Handle, unsigned char*, int, int);
    using SetBitCountFn = bool(MUCAM_CALL*)(Handle, int);
    using GetBinningCountFn = int(MUCAM_CALL*)(Handle);
    using GetBinningListFn = bool(MUCAM_CALL*)(Handle, int*, int*);
    using SetBinningIndexFn = bool(MUCAM_CALL*)(Handle, int);
    using SetTriggerTypeFn = bool(MUCAM_CALL*)(Handle, int);
    using GetExposureRangeFn = bool(MUCAM_CALL*)(Handle, float*, float*);
    using SetExposureFn = bool(MUCAM_CALL*)(Handle, float);
    using AutoExposureOnceFn = float(MUCAM_CALL*)(Handle, long);
    using SetAutoExposureFn = bool(MUCAM_CALL*)(Handle, int, float*);
    using SetRgbGainValueFn = bool(MUCAM_CALL*)(Handle, float, float, float, int*, int*, int*);
    using CalcWhiteBalanceFn = bool(MUCAM_CALL*)(Handle);
    using SetWhiteBalanceFn = bool(MUCAM_CALL*)(Handle, int);
    using GetBayerFormatFn = int(MUCAM_CALL*)(Handle);
    using GetBayerFn = bool(MUCAM_CALL*)(Handle, unsigned char*, unsigned long*);
    using BayerToRgbFn = bool(MUCAM_CALL*)(Handle, unsigned char*, int, int, int, int, unsigned char*);

    bool ResolveRequired();
    FARPROC ResolveAny(const char* ex_name, const char* legacy_name, bool required);
    FARPROC ResolveProc(const char* name, bool required);
    static std::wstring FormatWindowsError(DWORD error_code);
    static std::wstring Widen(const char* text);

    HMODULE module_ = nullptr;
    bool uses_ex_api_ = false;
    std::wstring last_error_;
    std::wstring loaded_path_;

    FindCameraFn find_camera_ = nullptr;
    ReleaseCameraFn release_camera_ = nullptr;
    GetTypeFn get_type_ = nullptr;
    OpenCameraFn open_camera_ = nullptr;
    CloseCameraFn close_camera_ = nullptr;
    IsConnectedFn is_connected_ = nullptr;
    GetFrameFormatFn get_frame_format_ = nullptr;
    GetFrameLegacyFn get_frame_legacy_ = nullptr;
    GetFrameExFn get_frame_ex_ = nullptr;
    SetBitCountFn set_bit_count_ = nullptr;
    GetBinningCountFn get_binning_count_ = nullptr;
    GetBinningListFn get_binning_list_ = nullptr;
    SetBinningIndexFn set_binning_index_ = nullptr;
    SetTriggerTypeFn set_trigger_type_ = nullptr;
    GetExposureRangeFn get_exposure_range_ = nullptr;
    SetExposureFn set_exposure_ = nullptr;
    AutoExposureOnceFn auto_exposure_once_ = nullptr;
    SetAutoExposureFn set_auto_exposure_ = nullptr;
    SetRgbGainValueFn set_rgb_gain_value_ = nullptr;
    CalcWhiteBalanceFn calc_white_balance_ = nullptr;
    SetWhiteBalanceFn set_white_balance_ = nullptr;
    GetBayerFormatFn get_bayer_format_ = nullptr;
    GetBayerFn get_bayer_ = nullptr;
    BayerToRgbFn bayer_to_rgb_ = nullptr;
};
