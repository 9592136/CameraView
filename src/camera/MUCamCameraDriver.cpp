#include "MUCamCameraDriver.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

MUCamCameraDriver::~MUCamCameraDriver()
{
    Close();
}

bool MUCamCameraDriver::Load()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return sdk_.Load();
}

std::wstring MUCamCameraDriver::LastError() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return sdk_.LastError();
}

CameraSdkDiagnostics MUCamCameraDriver::Diagnostics() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    CameraSdkDiagnostics diagnostics;
    diagnostics.loaded = sdk_.IsLoaded();
    diagnostics.uses_ex_api = sdk_.UsesExApi();
    diagnostics.has_exposure_control = sdk_.HasExposureControl();
    diagnostics.has_auto_exposure_control = sdk_.HasAutoExposureControl();
    diagnostics.has_gain_control = sdk_.HasGainControl();
    diagnostics.has_white_balance_control = sdk_.HasWhiteBalanceControl();
    diagnostics.has_bayer_readout = sdk_.HasBayerReadout();
    diagnostics.has_bayer_to_rgb = sdk_.HasBayerToRgb();
    diagnostics.has_bit_depth_control = sdk_.HasBitDepthControl();
    diagnostics.loaded_path = sdk_.LoadedPath();
    diagnostics.last_error = sdk_.LastError();
    return diagnostics;
}

std::vector<CameraDevice> MUCamCameraDriver::EnumerateDevices()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<CameraDevice> devices;
    if (!sdk_.Load()) {
        return devices;
    }

    std::vector<MUCamApi::Handle> cameras;
    for (;;) {
        MUCamApi::Handle found = sdk_.FindCamera();
        if (!found) {
            break;
        }
        CameraDevice device;
        device.index = static_cast<int>(devices.size());
        device.type = sdk_.GetType(found);
        device.display_name =
            L"Device " + std::to_wstring(device.index + 1) +
            L" | type " + std::to_wstring(device.type);
        cameras.push_back(found);
        devices.push_back(std::move(device));
    }

    for (MUCamApi::Handle camera : cameras) {
        sdk_.ReleaseCamera(camera);
    }
    return devices;
}

bool MUCamCameraDriver::Open(int device_index, float initial_exposure_ms)
{
    std::lock_guard<std::mutex> lock(mutex_);
    CloseLocked();
    if (device_index < 0 || !sdk_.Load()) {
        return false;
    }

    std::vector<MUCamApi::Handle> cameras;
    for (;;) {
        MUCamApi::Handle found = sdk_.FindCamera();
        if (!found) {
            break;
        }
        cameras.push_back(found);
    }
    if (device_index >= static_cast<int>(cameras.size())) {
        for (MUCamApi::Handle camera : cameras) {
            sdk_.ReleaseCamera(camera);
        }
        return false;
    }

    camera_ = cameras[static_cast<std::size_t>(device_index)];
    for (std::size_t index = 0; index < cameras.size(); ++index) {
        if (index != static_cast<std::size_t>(device_index)) {
            sdk_.ReleaseCamera(cameras[index]);
        }
    }

    open_info_ = CameraOpenInfo();
    open_info_.device_index = device_index;
    open_info_.type = sdk_.GetType(camera_);

    if (!sdk_.OpenCamera(camera_)) {
        CloseLocked();
        return false;
    }

    sdk_.SetTriggerType(camera_, 0);
    if (sdk_.HasBitDepthControl()) {
        sdk_.SetBitCount(camera_, 8);
    }

    const int binning_count = sdk_.GetBinningCount(camera_);
    if (binning_count <= 0) {
        CloseLocked();
        return false;
    }

    std::vector<int> widths(static_cast<std::size_t>(binning_count));
    std::vector<int> heights(static_cast<std::size_t>(binning_count));
    if (!sdk_.GetBinningList(camera_, widths.data(), heights.data())) {
        CloseLocked();
        return false;
    }

    constexpr int binning_index = 0;
    if (!sdk_.SetBinningIndex(camera_, binning_index)) {
        CloseLocked();
        return false;
    }

    open_info_.width = widths[binning_index];
    open_info_.height = heights[binning_index];
    frame_format_ = sdk_.GetFrameFormat(camera_);
    if (open_info_.width <= 0 || open_info_.height <= 0) {
        CloseLocked();
        return false;
    }

    const int input_channels = IsColorFormat(frame_format_) ? 3 : 1;
    raw_.assign(
        static_cast<std::size_t>(open_info_.width) *
        static_cast<std::size_t>(open_info_.height) *
        static_cast<std::size_t>(input_channels),
        0);
    rgb_.clear();
    ApplyExposureLocked(initial_exposure_ms);
    return true;
}

void MUCamCameraDriver::Close()
{
    std::lock_guard<std::mutex> lock(mutex_);
    CloseLocked();
}

bool MUCamCameraDriver::IsOpen() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return camera_ != nullptr;
}

bool MUCamCameraDriver::IsConnected() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return camera_ ? sdk_.IsConnected(camera_) : false;
}

CameraOpenInfo MUCamCameraDriver::OpenInfo() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return open_info_;
}

bool MUCamCameraDriver::HasExposureControl() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return sdk_.HasExposureControl();
}

bool MUCamCameraDriver::GetExposureRange(float& min_value, float& max_value) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return camera_ && sdk_.GetExposureRange(camera_, &min_value, &max_value);
}

bool MUCamCameraDriver::SetExposure(float value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return ApplyExposureLocked(value);
}

bool MUCamCameraDriver::HasAutoExposureControl() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return sdk_.HasAutoExposureControl();
}

bool MUCamCameraDriver::ApplyAutoExposure()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return camera_ && sdk_.ApplyAutoExposure(camera_);
}

bool MUCamCameraDriver::HasGainControl() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return sdk_.HasGainControl();
}

bool MUCamCameraDriver::SetGain(float value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    int red_index = 0;
    int green_index = 0;
    int blue_index = 0;
    return camera_ &&
           value > 0.0f &&
           sdk_.SetRgbGainValue(camera_, value, value, value, &red_index, &green_index, &blue_index);
}

bool MUCamCameraDriver::HasWhiteBalanceControl() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return sdk_.HasWhiteBalanceControl();
}

bool MUCamCameraDriver::ApplyWhiteBalance()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return camera_ && sdk_.ApplyWhiteBalance(camera_);
}

bool MUCamCameraDriver::GrabFrame(unsigned long long sequence, ImageFrame& frame)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!camera_ || raw_.empty() || open_info_.width <= 0 || open_info_.height <= 0) {
        return false;
    }

    unsigned long timestamp = 0;
    bool got_frame = false;
    int display_format = frame_format_;
    const unsigned char* display_source = raw_.data();

    int bayer_format = frame_format_;
    if (IsBayerFormat(frame_format_) && sdk_.HasBayerReadout()) {
        bayer_format = sdk_.GetBayerFormat(camera_);
        got_frame = sdk_.GetBayer(camera_, raw_.data(), &timestamp);
    } else {
        got_frame = sdk_.GetFrame(camera_, raw_.data(), open_info_.width, open_info_.height, &timestamp);
    }

    if (got_frame && IsBayerFormat(frame_format_) && sdk_.HasBayerToRgb()) {
        rgb_.assign(
            static_cast<std::size_t>(open_info_.width) *
            static_cast<std::size_t>(open_info_.height) * 3U,
            0);
        if (sdk_.BayerToRgb(camera_, raw_.data(), bayer_format, open_info_.width, open_info_.height, 8, rgb_.data())) {
            display_source = rgb_.data();
            display_format = MUCamApi::MUCAM_FORMAT_COLOR_RGB;
        } else {
            rgb_.clear();
        }
    }

    return got_frame && BuildDisplayFrame(
        display_source,
        display_format,
        open_info_.width,
        open_info_.height,
        timestamp,
        sequence,
        frame);
}

bool MUCamCameraDriver::IsBayerFormat(int format)
{
    return format >= MUCamApi::MUCAM_FORMAT_BAYER_GR_BG && format <= MUCamApi::MUCAM_FORMAT_BAYER_RG_GB;
}

bool MUCamCameraDriver::IsColorFormat(int format)
{
    return format == MUCamApi::MUCAM_FORMAT_COLOR_RGB || format == MUCamApi::MUCAM_FORMAT_COLOR_BGR;
}

bool MUCamCameraDriver::BuildDisplayFrame(
    const unsigned char* source,
    int source_format,
    int width,
    int height,
    unsigned long timestamp,
    unsigned long long sequence,
    ImageFrame& output)
{
    if (!source || width <= 0 || height <= 0) {
        return false;
    }

    output.width = width;
    output.height = height;
    output.timestamp = timestamp;
    output.sequence = sequence;
    output.stride = (width * 3 + 3) & ~3;
    output.bgr.assign(static_cast<std::size_t>(output.stride) * static_cast<std::size_t>(height), 0);

    for (int y = 0; y < height; ++y) {
        unsigned char* dst = output.bgr.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(output.stride);
        if (source_format == MUCamApi::MUCAM_FORMAT_COLOR_BGR) {
            std::memcpy(dst, source + static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 3U, static_cast<std::size_t>(width) * 3U);
        } else if (source_format == MUCamApi::MUCAM_FORMAT_COLOR_RGB) {
            const unsigned char* src = source + static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 3U;
            for (int x = 0; x < width; ++x) {
                dst[x * 3 + 0] = src[x * 3 + 2];
                dst[x * 3 + 1] = src[x * 3 + 1];
                dst[x * 3 + 2] = src[x * 3 + 0];
            }
        } else {
            const unsigned char* src = source + static_cast<std::size_t>(y) * static_cast<std::size_t>(width);
            for (int x = 0; x < width; ++x) {
                dst[x * 3 + 0] = src[x];
                dst[x * 3 + 1] = src[x];
                dst[x * 3 + 2] = src[x];
            }
        }
    }

    return true;
}

void MUCamCameraDriver::CloseLocked()
{
    if (camera_) {
        sdk_.CloseCamera(camera_);
        sdk_.ReleaseCamera(camera_);
        camera_ = nullptr;
    }
    open_info_ = CameraOpenInfo();
    raw_.clear();
    rgb_.clear();
}

bool MUCamCameraDriver::ApplyExposureLocked(float value)
{
    if (!camera_ || !sdk_.HasExposureControl()) {
        return false;
    }

    float min_value = 0.0f;
    float max_value = 0.0f;
    float clamped = value;
    if (sdk_.GetExposureRange(camera_, &min_value, &max_value)) {
        clamped = std::clamp(value, min_value, max_value);
    }
    return sdk_.SetExposure(camera_, clamped);
}
