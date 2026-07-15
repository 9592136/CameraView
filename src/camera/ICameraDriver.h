#pragma once

#include "CameraDevice.h"
#include "../domain/ImageFrame.h"

#include <string>
#include <vector>

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

    virtual bool HasExposureControl() const = 0;
    virtual bool GetExposureRange(float& min_value, float& max_value) const = 0;
    virtual bool SetExposure(float value) = 0;
    virtual bool HasAutoExposureControl() const = 0;
    virtual bool ApplyAutoExposure() = 0;
    virtual bool HasGainControl() const = 0;
    virtual bool SetGain(float value) = 0;
    virtual bool HasWhiteBalanceControl() const = 0;
    virtual bool ApplyWhiteBalance() = 0;

    virtual bool GrabFrame(unsigned long long sequence, ImageFrame& frame) = 0;
};
