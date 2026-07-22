#include "MUCamCameraDriver.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

namespace {

std::size_t CaptureBufferByteCount(int width, int height, int channels, int bytes_per_channel)
{
    const std::size_t pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    const std::size_t source_bytes =
        pixels * static_cast<std::size_t>((std::max)(channels, 1)) *
        static_cast<std::size_t>((std::max)(bytes_per_channel, 1));
    const std::size_t display_stride = static_cast<std::size_t>((width * 3 + 3) & ~3);
    return (std::max)(source_bytes, display_stride * static_cast<std::size_t>(height));
}

unsigned char SampleTo8Bit(const unsigned char* sample, int bytes_per_channel)
{
    if (!sample) {
        return 0;
    }
    if (bytes_per_channel <= 1) {
        return sample[0];
    }

    const unsigned int value =
        static_cast<unsigned int>(sample[0]) |
        (static_cast<unsigned int>(sample[1]) << 8);
    return static_cast<unsigned char>((value >> 8) & 0xFFU);
}

} // namespace

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
    input_bytes_per_channel_ = 1;
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
        CaptureBufferByteCount(open_info_.width, open_info_.height, input_channels, 2),
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
    int display_bytes_per_channel = input_bytes_per_channel_;
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
            display_bytes_per_channel = 1;
        } else {
            rgb_.clear();
        }
    }

    return got_frame && BuildDisplayFrame(
        display_source,
        display_format,
        display_bytes_per_channel,
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
    int source_bytes_per_channel,
    int width,
    int height,
    unsigned long timestamp,
    unsigned long long sequence,
    ImageFrame& output)
{
    if (!source || width <= 0 || height <= 0) {
        return false;
    }

    const int bytes_per_channel = source_bytes_per_channel > 1 ? 2 : 1;

    output.width = width;
    output.height = height;
    output.timestamp = timestamp;
    output.sequence = sequence;
    output.stride = (width * 3 + 3) & ~3;
    output.bgr.assign(static_cast<std::size_t>(output.stride) * static_cast<std::size_t>(height), 0);

    for (int y = 0; y < height; ++y) {
        unsigned char* dst = output.bgr.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(output.stride);
        if (source_format == MUCamApi::MUCAM_FORMAT_COLOR_BGR) {
            const unsigned char* src =
                source + static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 3U *
                static_cast<std::size_t>(bytes_per_channel);
            if (bytes_per_channel == 1) {
                std::memcpy(dst, src, static_cast<std::size_t>(width) * 3U);
            } else {
                for (int x = 0; x < width; ++x) {
                    const std::size_t pixel = static_cast<std::size_t>(x) * 3U * 2U;
                    dst[x * 3 + 0] = SampleTo8Bit(src + pixel + 0U, bytes_per_channel);
                    dst[x * 3 + 1] = SampleTo8Bit(src + pixel + 2U, bytes_per_channel);
                    dst[x * 3 + 2] = SampleTo8Bit(src + pixel + 4U, bytes_per_channel);
                }
            }
        } else if (source_format == MUCamApi::MUCAM_FORMAT_COLOR_RGB) {
            const unsigned char* src =
                source + static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 3U *
                static_cast<std::size_t>(bytes_per_channel);
            for (int x = 0; x < width; ++x) {
                const std::size_t pixel =
                    static_cast<std::size_t>(x) * 3U * static_cast<std::size_t>(bytes_per_channel);
                dst[x * 3 + 0] = SampleTo8Bit(src + pixel + 2U * static_cast<std::size_t>(bytes_per_channel), bytes_per_channel);
                dst[x * 3 + 1] = SampleTo8Bit(src + pixel + 1U * static_cast<std::size_t>(bytes_per_channel), bytes_per_channel);
                dst[x * 3 + 2] = SampleTo8Bit(src + pixel + 0U, bytes_per_channel);
            }
        } else {
            const unsigned char* src =
                source + static_cast<std::size_t>(y) * static_cast<std::size_t>(width) *
                static_cast<std::size_t>(bytes_per_channel);
            for (int x = 0; x < width; ++x) {
                const unsigned char gray = SampleTo8Bit(
                    src + static_cast<std::size_t>(x) * static_cast<std::size_t>(bytes_per_channel),
                    bytes_per_channel);
                dst[x * 3 + 0] = gray;
                dst[x * 3 + 1] = gray;
                dst[x * 3 + 2] = gray;
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
    input_bytes_per_channel_ = 1;
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
