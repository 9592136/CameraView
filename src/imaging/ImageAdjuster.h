#pragma once

#include "../domain/ImageFrame.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

struct ImageAdjustParams
{
    int brightness = 0;     // -100 .. 100
    int contrast = 0;       // -100 .. 100
    int gamma_tenths = 10;  // 1 .. 30, gamma = value / 10
    int window_level = 128; // 0 .. 255
    int window_width = 256; // 1 .. 256

    bool IsIdentity() const
    {
        return brightness == 0 && contrast == 0 && gamma_tenths == 10 &&
            window_level == 128 && window_width == 256;
    }

    bool IsWindowIdentity() const
    {
        return window_level == 128 && window_width == 256;
    }
};

namespace image_adjuster_detail {

inline bool HasReadableImageData(const ImageFrame& frame)
{
    if (!frame.IsValid()) {
        return false;
    }

    const std::size_t required =
        static_cast<std::size_t>(frame.height) * static_cast<std::size_t>(frame.stride);
    return frame.bgr.size() >= required;
}

} // namespace image_adjuster_detail

inline void ApplyAdjustments(const ImageFrame& src, ImageFrame& dst, const ImageAdjustParams& params)
{
    if (!image_adjuster_detail::HasReadableImageData(src)) {
        dst = ImageFrame{};
        return;
    }

    if (params.IsIdentity()) {
        dst = src;
        return;
    }

    const std::size_t total = static_cast<std::size_t>(src.height) * static_cast<std::size_t>(src.stride);
    std::vector<unsigned char> adjusted(total);

    const int brightness = std::clamp(params.brightness, -100, 100);
    const double contrast = 1.0 + std::clamp(params.contrast, -100, 100) / 100.0;
    const double contrast_midpoint = (1.0 - contrast) * 128.0;
    const double gamma = std::clamp(params.gamma_tenths, 1, 30) / 10.0;
    const double gamma_inverse = 1.0 / gamma;

    unsigned char gamma_lut[256];
    for (int i = 0; i < 256; ++i) {
        const double value = std::pow(i / 255.0, gamma_inverse) * 255.0;
        gamma_lut[i] = static_cast<unsigned char>(std::clamp(static_cast<int>(value + 0.5), 0, 255));
    }

    const bool use_window = !params.IsWindowIdentity();
    const int window_level = std::clamp(params.window_level, 0, 255);
    const int window_width = std::clamp(params.window_width, 1, 256);
    const int low_in = (std::max)(window_level - window_width / 2, 0);
    const int high_in = (std::min)(window_level + window_width / 2, 255);
    const double window_range = static_cast<double>(high_in - low_in);
    const double window_scale = (window_range > 0.0) ? 255.0 / window_range : 1.0;

    const unsigned char* src_data = src.bgr.data();
    for (std::size_t i = 0; i < total; ++i) {
        int value = static_cast<int>(src_data[i]) + brightness;
        value = std::clamp(value, 0, 255);

        double adjusted_value = contrast * value + contrast_midpoint;
        adjusted_value = std::clamp(adjusted_value, 0.0, 255.0);
        value = static_cast<int>(adjusted_value + 0.5);

        if (use_window) {
            if (value <= low_in) {
                value = 0;
            } else if (value >= high_in) {
                value = 255;
            } else {
                value = static_cast<int>((value - low_in) * window_scale + 0.5);
                value = std::clamp(value, 0, 255);
            }
        }

        adjusted[i] = gamma_lut[value];
    }

    dst.width = src.width;
    dst.height = src.height;
    dst.stride = src.stride;
    dst.sequence = src.sequence;
    dst.timestamp = src.timestamp;
    dst.bgr = std::move(adjusted);
}

inline ImageFrame ApplyAdjustments(const ImageFrame& src, const ImageAdjustParams& params)
{
    ImageFrame dst;
    ApplyAdjustments(src, dst, params);
    return dst;
}
