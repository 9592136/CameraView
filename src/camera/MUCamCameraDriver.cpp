#include "MUCamCameraDriver.h"

#include <algorithm>
#include <cmath>
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

float NearestSupportedGain(float value, const std::vector<float>& supported)
{
    if (supported.empty()) return value;
    return *std::min_element(
        supported.begin(), supported.end(),
        [value](float left, float right) {
            return std::abs(left - value) < std::abs(right - value);
        });
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

    capabilities_ = {};
    capabilities_.frame_format = frame_format_;
    capabilities_.color = IsColorFormat(frame_format_) || IsBayerFormat(frame_format_);
    capabilities_.has_exposure = sdk_.HasExposureControl();
    capabilities_.has_auto_exposure = sdk_.HasAutoExposureControl();
    capabilities_.has_gain = sdk_.HasGainControl();
    capabilities_.has_offset = sdk_.HasOffsetControl();
    capabilities_.has_white_balance = sdk_.HasWhiteBalanceControl() && capabilities_.color;
    capabilities_.has_roi = sdk_.HasRoiControl();
    capabilities_.has_trigger = sdk_.HasTriggerControl();
    // Output orientation is implemented in the display-frame conversion.  A
    // number of MUCam models export setFlip/setMirror but reject the calls (or
    // silently ignore them), so advertising the SDK entry point as a device
    // capability is not reliable.
    capabilities_.has_flip = true;
    capabilities_.has_mirror = true;
    for (int index = 0; index < binning_count; ++index) {
        capabilities_.resolutions.push_back({index, widths[index], heights[index]});
    }
    float exposure_minimum = 0.01f;
    float exposure_maximum = 10000.0f;
    if (sdk_.GetExposureRange(camera_, &exposure_minimum, &exposure_maximum)) {
        capabilities_.exposure_minimum = exposure_minimum;
        capabilities_.exposure_maximum = exposure_maximum;
    }
    // Some MUCam firmware revisions stop returning frames when gain/offset
    // capability functions are queried immediately after OpenCamera.  Keep
    // stream start on the SDK's proven path and use conservative defaults;
    // the setters still clamp values and report failures normally.
    capabilities_.gain_values.clear();
    capabilities_.offset_minimum = -255;
    capabilities_.offset_maximum = 255;
    configuration_ = {};
    configuration_.resolution_index = binning_index;
    configuration_.exposure_ms = std::clamp(
        initial_exposure_ms, capabilities_.exposure_minimum, capabilities_.exposure_maximum);
    RebuildBuffersLocked();
    ApplyExposureLocked(configuration_.exposure_ms);
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

CameraCapabilities MUCamCameraDriver::Capabilities() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return capabilities_;
}

CameraConfiguration MUCamCameraDriver::Configuration() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return configuration_;
}

bool MUCamCameraDriver::Configure(const CameraConfiguration& configuration)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!camera_) {
        return false;
    }

    const CameraConfiguration previous = configuration_;
    const bool roi_unchanged =
        configuration.roi.x == previous.roi.x &&
        configuration.roi.y == previous.roi.y &&
        configuration.roi.width == previous.roi.width &&
        configuration.roi.height == previous.roi.height;
    const bool only_orientation_changed =
        configuration.resolution_index == previous.resolution_index &&
        roi_unchanged &&
        configuration.trigger_mode == previous.trigger_mode &&
        std::abs(configuration.exposure_ms - previous.exposure_ms) < 0.0001f &&
        std::abs(configuration.red_gain - previous.red_gain) < 0.0001f &&
        std::abs(configuration.green_gain - previous.green_gain) < 0.0001f &&
        std::abs(configuration.blue_gain - previous.blue_gain) < 0.0001f &&
        configuration.red_offset == previous.red_offset &&
        configuration.green_offset == previous.green_offset &&
        configuration.blue_offset == previous.blue_offset;
    if (only_orientation_changed) {
        configuration_.vertical_flip = configuration.vertical_flip;
        configuration_.horizontal_mirror = configuration.horizontal_mirror;
        return true;
    }
    if (ApplyConfigurationLocked(configuration)) {
        return true;
    }

    // A structural setting may already have reached the SDK. Restore the last
    // known-good configuration before reporting failure to the UI.
    ApplyConfigurationLocked(previous);
    return false;
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
    return SetRgbGain(value, value, value);
}

bool MUCamCameraDriver::SetRgbGain(float red, float green, float blue)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!camera_ || !capabilities_.has_gain || red <= 0.0f || green <= 0.0f || blue <= 0.0f) {
        return false;
    }
    red = NearestSupportedGain(red, capabilities_.gain_values);
    green = NearestSupportedGain(green, capabilities_.gain_values);
    blue = NearestSupportedGain(blue, capabilities_.gain_values);
    int red_index = 0;
    int green_index = 0;
    int blue_index = 0;
    if (!sdk_.SetRgbGainValue(camera_, red, green, blue, &red_index, &green_index, &blue_index)) {
        return false;
    }
    configuration_.red_gain = red;
    configuration_.green_gain = green;
    configuration_.blue_gain = blue;
    return true;
}

bool MUCamCameraDriver::SetRgbOffset(int red, int green, int blue)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!camera_ || !capabilities_.has_offset) {
        return false;
    }
    red = std::clamp(red, capabilities_.offset_minimum, capabilities_.offset_maximum);
    green = std::clamp(green, capabilities_.offset_minimum, capabilities_.offset_maximum);
    blue = std::clamp(blue, capabilities_.offset_minimum, capabilities_.offset_maximum);
    if (!sdk_.SetRgbOffset(camera_, red, green, blue)) {
        return false;
    }
    configuration_.red_offset = red;
    configuration_.green_offset = green;
    configuration_.blue_offset = blue;
    return true;
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

bool MUCamCameraDriver::GrabFrame(uint64_t sequence, ImageFrame& frame)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!camera_ || raw_.empty() || open_info_.width <= 0 || open_info_.height <= 0) {
        return false;
    }

    unsigned long sdk_timestamp = 0;
    uint32_t timestamp = 0;
    bool got_frame = false;
    int display_format = frame_format_;
    int display_bytes_per_channel = input_bytes_per_channel_;
    const unsigned char* display_source = raw_.data();

    int bayer_format = frame_format_;
    if (IsBayerFormat(frame_format_) && sdk_.HasBayerReadout()) {
        bayer_format = sdk_.GetBayerFormat(camera_);
        got_frame = sdk_.GetBayer(camera_, raw_.data(), &sdk_timestamp);
    } else {
        got_frame = sdk_.GetFrame(camera_, raw_.data(), open_info_.width, open_info_.height, &sdk_timestamp);
    }
    timestamp = static_cast<uint32_t>(sdk_timestamp);

    if (got_frame && IsBayerFormat(frame_format_) && sdk_.HasBayerToRgb()) {
        rgb_.resize(
            static_cast<std::size_t>(open_info_.width) *
            static_cast<std::size_t>(open_info_.height) * 3U);
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
        configuration_.vertical_flip,
        configuration_.horizontal_mirror,
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
    uint32_t timestamp,
    uint64_t sequence,
    bool vertical_flip,
    bool horizontal_mirror,
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
    output.bgr.resize(static_cast<std::size_t>(output.stride) * static_cast<std::size_t>(height));

    for (int y = 0; y < height; ++y) {
        unsigned char* dst = output.bgr.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(output.stride);
        const int source_y = vertical_flip ? height - 1 - y : y;
        if (source_format == MUCamApi::MUCAM_FORMAT_COLOR_BGR) {
            const unsigned char* src =
                source + static_cast<std::size_t>(source_y) * static_cast<std::size_t>(width) * 3U *
                static_cast<std::size_t>(bytes_per_channel);
            if (bytes_per_channel == 1 && !horizontal_mirror) {
                std::memcpy(dst, src, static_cast<std::size_t>(width) * 3U);
            } else {
                for (int x = 0; x < width; ++x) {
                    const int source_x = horizontal_mirror ? width - 1 - x : x;
                    const std::size_t pixel = static_cast<std::size_t>(source_x) * 3U *
                        static_cast<std::size_t>(bytes_per_channel);
                    dst[x * 3 + 0] = SampleTo8Bit(src + pixel + 0U, bytes_per_channel);
                    dst[x * 3 + 1] = SampleTo8Bit(
                        src + pixel + static_cast<std::size_t>(bytes_per_channel), bytes_per_channel);
                    dst[x * 3 + 2] = SampleTo8Bit(
                        src + pixel + 2U * static_cast<std::size_t>(bytes_per_channel), bytes_per_channel);
                }
            }
        } else if (source_format == MUCamApi::MUCAM_FORMAT_COLOR_RGB) {
            const unsigned char* src =
                source + static_cast<std::size_t>(source_y) * static_cast<std::size_t>(width) * 3U *
                static_cast<std::size_t>(bytes_per_channel);
            for (int x = 0; x < width; ++x) {
                const int source_x = horizontal_mirror ? width - 1 - x : x;
                const std::size_t pixel =
                    static_cast<std::size_t>(source_x) * 3U * static_cast<std::size_t>(bytes_per_channel);
                dst[x * 3 + 0] = SampleTo8Bit(src + pixel + 2U * static_cast<std::size_t>(bytes_per_channel), bytes_per_channel);
                dst[x * 3 + 1] = SampleTo8Bit(src + pixel + 1U * static_cast<std::size_t>(bytes_per_channel), bytes_per_channel);
                dst[x * 3 + 2] = SampleTo8Bit(src + pixel + 0U, bytes_per_channel);
            }
        } else {
            const unsigned char* src =
                source + static_cast<std::size_t>(source_y) * static_cast<std::size_t>(width) *
                static_cast<std::size_t>(bytes_per_channel);
            for (int x = 0; x < width; ++x) {
                const int source_x = horizontal_mirror ? width - 1 - x : x;
                const unsigned char gray = SampleTo8Bit(
                    src + static_cast<std::size_t>(source_x) * static_cast<std::size_t>(bytes_per_channel),
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
    capabilities_ = CameraCapabilities();
    configuration_ = CameraConfiguration();
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
    if (!sdk_.SetExposure(camera_, clamped)) {
        return false;
    }
    configuration_.exposure_ms = clamped;
    return true;
}

bool MUCamCameraDriver::ApplyConfigurationLocked(const CameraConfiguration& requested)
{
    if (!camera_ || capabilities_.resolutions.empty()) {
        return false;
    }

    const auto resolution = std::find_if(
        capabilities_.resolutions.begin(),
        capabilities_.resolutions.end(),
        [&requested](const CameraResolutionOption& option) {
            return option.index == requested.resolution_index;
        });
    if (resolution == capabilities_.resolutions.end() ||
        resolution->width <= 0 || resolution->height <= 0) {
        return false;
    }

    // Setting Binning first restores the corresponding full frame, so ROI
    // coordinates never become nested inside the previous ROI.
    if (!sdk_.SetBinningIndex(camera_, resolution->index)) {
        return false;
    }

    const CameraConfiguration previous = configuration_;
    CameraConfiguration actual = requested;
    actual.resolution_index = resolution->index;
    open_info_.width = resolution->width;
    open_info_.height = resolution->height;

    if (requested.roi.Enabled()) {
        if (!capabilities_.has_roi) {
            return false;
        }
        int left = std::clamp(requested.roi.x, 0, resolution->width - 1);
        int top = std::clamp(requested.roi.y, 0, resolution->height - 1);
        const int width = std::clamp(requested.roi.width, 1, resolution->width - left);
        const int height = std::clamp(requested.roi.height, 1, resolution->height - top);
        int right = left + width - 1;
        int bottom = top + height - 1;
        if (!sdk_.SetRoi(camera_, &top, &left, &bottom, &right)) {
            return false;
        }
        open_info_.width = right - left + 1;
        open_info_.height = bottom - top + 1;
        actual.roi = {left, top, open_info_.width, open_info_.height};
    } else {
        actual.roi = {};
    }

    if (requested.trigger_mode != CameraTriggerMode::Free && !capabilities_.has_trigger) {
        return false;
    }
    if (capabilities_.has_trigger &&
        !sdk_.SetTriggerType(camera_, static_cast<int>(requested.trigger_mode))) {
        return false;
    }
    // Flip and mirror are applied while building the BGR display frame.  This
    // keeps their behavior consistent across MUCam models and avoids a full
    // configuration rollback when firmware rejects the optional SDK calls.

    float exposure_minimum = capabilities_.exposure_minimum;
    float exposure_maximum = capabilities_.exposure_maximum;
    if (capabilities_.has_exposure &&
        sdk_.GetExposureRange(camera_, &exposure_minimum, &exposure_maximum)) {
        capabilities_.exposure_minimum = exposure_minimum;
        capabilities_.exposure_maximum = exposure_maximum;
    }
    actual.exposure_ms = std::clamp(
        requested.exposure_ms, capabilities_.exposure_minimum, capabilities_.exposure_maximum);
    configuration_ = actual;
    if (capabilities_.has_exposure && !ApplyExposureLocked(actual.exposure_ms)) {
        return false;
    }

    const bool gain_changed = actual.red_gain != previous.red_gain ||
        actual.green_gain != previous.green_gain || actual.blue_gain != previous.blue_gain;
    if (capabilities_.color && capabilities_.has_gain && gain_changed) {
        actual.red_gain = NearestSupportedGain(actual.red_gain, capabilities_.gain_values);
        actual.green_gain = NearestSupportedGain(actual.green_gain, capabilities_.gain_values);
        actual.blue_gain = NearestSupportedGain(actual.blue_gain, capabilities_.gain_values);
        int red_index = 0;
        int green_index = 0;
        int blue_index = 0;
        if (!sdk_.SetRgbGainValue(
                camera_, actual.red_gain, actual.green_gain, actual.blue_gain,
                &red_index, &green_index, &blue_index)) {
            return false;
        }
    }
    const bool offset_changed = actual.red_offset != previous.red_offset ||
        actual.green_offset != previous.green_offset || actual.blue_offset != previous.blue_offset;
    if (capabilities_.color && capabilities_.has_offset && offset_changed) {
        actual.red_offset = std::clamp(
            actual.red_offset, capabilities_.offset_minimum, capabilities_.offset_maximum);
        actual.green_offset = std::clamp(
            actual.green_offset, capabilities_.offset_minimum, capabilities_.offset_maximum);
        actual.blue_offset = std::clamp(
            actual.blue_offset, capabilities_.offset_minimum, capabilities_.offset_maximum);
        if (!sdk_.SetRgbOffset(
                camera_, actual.red_offset, actual.green_offset, actual.blue_offset)) {
            return false;
        }
    }

    configuration_ = actual;
    RebuildBuffersLocked();
    return true;
}

void MUCamCameraDriver::RebuildBuffersLocked()
{
    const int input_channels = IsColorFormat(frame_format_) ? 3 : 1;
    raw_.assign(
        CaptureBufferByteCount(open_info_.width, open_info_.height, input_channels, 2),
        0);
    rgb_.clear();
}
