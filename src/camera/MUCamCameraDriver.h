#pragma once

#include "ICameraDriver.h"
#include "../MUCamApi.h"

#include <mutex>
#include <vector>

class MUCamCameraDriver final : public ICameraDriver {
public:
    ~MUCamCameraDriver() override;

    bool Load() override;
    std::wstring LastError() const override;
    CameraSdkDiagnostics Diagnostics() const override;
    std::vector<CameraDevice> EnumerateDevices() override;

    bool Open(int device_index, float initial_exposure_ms) override;
    void Close() override;
    bool IsOpen() const override;
    bool IsConnected() const override;
    CameraOpenInfo OpenInfo() const override;

    bool HasExposureControl() const override;
    bool GetExposureRange(float& min_value, float& max_value) const override;
    bool SetExposure(float value) override;
    bool HasAutoExposureControl() const override;
    bool ApplyAutoExposure() override;
    bool HasGainControl() const override;
    bool SetGain(float value) override;
    bool HasWhiteBalanceControl() const override;
    bool ApplyWhiteBalance() override;

    bool GrabFrame(unsigned long long sequence, ImageFrame& frame) override;

private:
    static bool IsBayerFormat(int format);
    static bool IsColorFormat(int format);
    static bool BuildDisplayFrame(
        const unsigned char* source,
        int source_format,
        int source_bytes_per_channel,
        int width,
        int height,
        unsigned long timestamp,
        unsigned long long sequence,
        ImageFrame& output);

    void CloseLocked();
    bool ApplyExposureLocked(float value);

    mutable std::mutex mutex_;
    MUCamApi sdk_;
    MUCamApi::Handle camera_ = nullptr;
    CameraOpenInfo open_info_;
    int frame_format_ = MUCamApi::MUCAM_FORMAT_COLOR_BGR;
    int input_bytes_per_channel_ = 1;
    std::vector<unsigned char> raw_;
    std::vector<unsigned char> rgb_;
};
