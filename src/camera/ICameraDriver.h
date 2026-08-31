#pragma once

#include "CameraDevice.h"
#include "../domain/ImageFrame.h"

#include <cstdint>
#include <string>
#include <vector>

enum class CameraTriggerMode {
    Free = 0,
    Software = 1,
    HardwareRise = 2,
    HardwareFall = 3
};

struct CameraResolutionOption {
    int index = 0;
    int width = 0;
    int height = 0;
};

struct CameraRoi {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool Enabled() const { return width > 0 && height > 0; }
};

struct CameraCapabilities {
    std::vector<CameraResolutionOption> resolutions;
    float exposure_minimum = 0.01f;
    float exposure_maximum = 10000.0f;
    std::vector<float> gain_values;
    int offset_minimum = 0;
    int offset_maximum = 0;
    int frame_format = 5;
    bool color = true;
    bool has_exposure = false;
    bool has_auto_exposure = false;
    bool has_gain = false;
    bool has_offset = false;
    bool has_white_balance = false;
    bool has_roi = false;
    bool has_trigger = false;
    bool has_flip = false;
    bool has_mirror = false;
};

struct CameraConfiguration {
    int resolution_index = 0;
    CameraRoi roi;
    CameraTriggerMode trigger_mode = CameraTriggerMode::Free;
    bool vertical_flip = false;
    bool horizontal_mirror = false;
    float exposure_ms = 10.0f;
    float red_gain = 1.0f;
    float green_gain = 1.0f;
    float blue_gain = 1.0f;
    int red_offset = 0;
    int green_offset = 0;
    int blue_offset = 0;
};

struct CameraSdkDiagnostics {
    bool loaded = false;
    bool uses_ex_api = false;
    bool has_exposure_control = false;
    bool has_auto_exposure_control = false;
    bool has_gain_control = false;
    bool has_white_balance_control = false;
    bool has_bayer_readout = false;
    bool has_bayer_to_rgb = false;
    bool has_bit_depth_control = false;
    std::wstring loaded_path;
    std::wstring last_error;
};

struct CameraOpenInfo {
    int device_index = -1;
    int type = -1;
    int width = 0;
    int height = 0;
};

class ICameraDriver {
public:
    virtual ~ICameraDriver() = default;

    virtual bool Load() = 0;
    virtual std::wstring LastError() const = 0;
    virtual CameraSdkDiagnostics Diagnostics() const = 0;
    virtual std::vector<CameraDevice> EnumerateDevices() = 0;

    virtual bool Open(int device_index, float initial_exposure_ms) = 0;
    virtual void Close() = 0;
    virtual bool IsOpen() const = 0;
    virtual bool IsConnected() const = 0;
    virtual CameraOpenInfo OpenInfo() const = 0;
    virtual CameraCapabilities Capabilities() const = 0;
    virtual CameraConfiguration Configuration() const = 0;
    virtual bool Configure(const CameraConfiguration& configuration) = 0;

    virtual bool HasExposureControl() const = 0;
    virtual bool GetExposureRange(float& min_value, float& max_value) const = 0;
    virtual bool SetExposure(float value) = 0;
    virtual bool HasAutoExposureControl() const = 0;
    virtual bool ApplyAutoExposure() = 0;
    virtual bool HasGainControl() const = 0;
    virtual bool SetGain(float value) = 0;
    virtual bool SetRgbGain(float red, float green, float blue) = 0;
    virtual bool SetRgbOffset(int red, int green, int blue) = 0;
    virtual bool HasWhiteBalanceControl() const = 0;
    virtual bool ApplyWhiteBalance() = 0;

    virtual bool GrabFrame(uint64_t sequence, ImageFrame& frame) = 0;
};
