#pragma once

#include "ICameraDriver.h"
#include "../MUCamApi.h"

#include <cstdint>
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
    CameraCapabilities Capabilities() const override;
    CameraConfiguration Configuration() const override;
    bool Configure(const CameraConfiguration& configuration) override;

    bool HasExposureControl() const override;
    bool GetExposureRange(float& min_value, float& max_value) const override;
    bool SetExposure(float value) override;
    bool HasAutoExposureControl() const override;
    bool ApplyAutoExposure() override;
    bool HasGainControl() const override;
    bool SetGain(float value) override;
    bool SetRgbGain(float red, float green, float blue) override;
    bool SetRgbOffset(int red, int green, int blue) override;
    bool HasWhiteBalanceControl() const override;
    bool ApplyWhiteBalance() override;

    bool GrabFrame(uint64_t sequence, ImageFrame& frame) override;

private:
    static bool IsBayerFormat(int format);
    static bool IsColorFormat(int format);
    static bool BuildDisplayFrame(
        const unsigned char* source,
        int source_format,
        int source_bytes_per_channel,
        int width,
        int height,
        uint32_t timestamp,
        uint64_t sequence,
        bool vertical_flip,
        bool horizontal_mirror,
        ImageFrame& output);

    void CloseLocked();
    bool ApplyExposureLocked(float value);
    bool ApplyConfigurationLocked(const CameraConfiguration& configuration);
    void RebuildBuffersLocked();

    mutable std::mutex mutex_;
    MUCamApi sdk_;
    MUCamApi::Handle camera_ = nullptr;
    CameraOpenInfo open_info_;
    CameraCapabilities capabilities_;
    CameraConfiguration configuration_;
    int frame_format_ = MUCamApi::MUCAM_FORMAT_COLOR_BGR;
    int input_bytes_per_channel_ = 1;
    std::vector<unsigned char> raw_;
    std::vector<unsigned char> rgb_;
};
