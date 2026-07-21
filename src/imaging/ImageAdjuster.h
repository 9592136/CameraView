#pragma once

#include "../domain/ImageFrame.h"

#include <algorithm>
#include <cmath>

/// Image adjustment parameters.
struct ImageAdjustParams
{
    int brightness = 0;     ///< -100 .. 100,  default 0
    int contrast = 0;       ///< -100 .. 100,  default 0  (0 = 1.0× multiplier)
    int gamma_tenths = 10;  ///<   1 ..  30,  default 10 (gamma = value/10, 0.1 .. 3.0)
    int window_level = 128; ///<   0 .. 255,  default 128  (centre of the intensity window)
    int window_width = 256; ///<   1 .. 256,  default 256  (width of the intensity window)

    bool IsIdentity() const
    {
        return brightness == 0 && contrast == 0 && gamma_tenths == 10
            && window_level == 128 && window_width == 256;
    }

    bool IsWindowIdentity() const
    {
        return window_level == 128 && window_width == 256;
    }
};

/// Applies brightness / contrast / window-level / gamma adjustments to the destination frame.
/// Order: brightness → contrast → window-level/window-width → gamma.
inline void ApplyAdjustments(const ImageFrame& src, ImageFrame& dst, const ImageAdjustParams& params)
{
    if (params.IsIdentity()) {
        return;
    }

    dst.width = src.width;
    dst.height = src.height;
    dst.stride = src.stride;
    dst.sequence = src.sequence;
    dst.timestamp = src.timestamp;

    const std::size_t total = static_cast<std::size_t>(src.height) * static_cast<std::size_t>(src.stride);
    dst.bgr.resize(total);

    const int b = params.brightness;
    const double c = 1.0 + params.contrast / 100.0;   // 0.0 .. 2.0
    const double cMid = (1.0 - c) * 128.0;
    const double g = params.gamma_tenths / 10.0;       // 0.1 .. 3.0
    const double gInv = 1.0 / g;

    // Pre-compute gamma LUT (8-bit)
    unsigned char gamma_lut[256];
    for (int i = 0; i < 256; ++i) {
        double v = std::pow(i / 255.0, gInv) * 255.0;
        gamma_lut[i] = static_cast<unsigned char>(std::clamp(static_cast<int>(v + 0.5), 0, 255));
    }

    // Window-level / window-width pre-computation
    const bool use_window = !params.IsWindowIdentity();
    const int wl = params.window_level;                       // centre  (0..255)
    const int ww = params.window_width;                       // width   (1..256)
    const int low_in = (std::max)(wl - ww / 2, 0);
    const int high_in = (std::min)(wl + ww / 2, 255);
    const double ww_range = static_cast<double>(high_in - low_in);
    const double ww_scale = (ww_range > 0.0) ? 255.0 / ww_range : 1.0;

    const unsigned char* src_data = src.bgr.data();
    unsigned char* dst_data = dst.bgr.data();

    for (std::size_t i = 0; i < total; ++i) {
        // Step 1: brightness
        int val = static_cast<int>(src_data[i]) + b;
        val = std::clamp(val, 0, 255);

        // Step 2: contrast
        double fval = c * val + cMid;
        fval = std::clamp(fval, 0.0, 255.0);
        val = static_cast<int>(fval + 0.5);

        // Step 3: window-level / window-width
        if (use_window) {
            if (val <= low_in) {
                val = 0;
            } else if (val >= high_in) {
                val = 255;
            } else {
                val = static_cast<int>((val - low_in) * ww_scale + 0.5);
                val = std::clamp(val, 0, 255);
            }
        }

        // Step 4: gamma (LUT)
        dst_data[i] = gamma_lut[val];
    }
}

/// Applies adjustments and returns a new adjusted frame.
inline ImageFrame ApplyAdjustments(const ImageFrame& src, const ImageAdjustParams& params)
{
    ImageFrame dst;
    ApplyAdjustments(src, dst, params);
    return dst;
}
