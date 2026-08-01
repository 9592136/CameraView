#include "HistogramCalculator.h"

#include <algorithm>
#include <cstddef>

namespace {

bool HasReadableBgrRows(const ImageFrame& frame)
{
    if (!frame.IsValid() || frame.stride < frame.width * 3) {
        return false;
    }

    const std::size_t row_stride = static_cast<std::size_t>(frame.stride);
    const std::size_t last_row = static_cast<std::size_t>(frame.height - 1) * row_stride;
    const std::size_t required = last_row + static_cast<std::size_t>(frame.width) * 3U;
    return frame.bgr.size() >= required;
}

} // namespace

HistogramData ComputeHistogram(const ImageFrame& frame, HistogramChannel channel)
{
    HistogramData result;
    if (!HasReadableBgrRows(frame)) {
        return result;
    }

    const int width = frame.width;
    const int height = frame.height;
    const int stride = frame.stride;
    const unsigned char* data = frame.bgr.data();

    auto& bins = result.bins;

    // Track min/max bin index and pixel sum during the main pixel pass,
    // avoiding separate 256-bin sweeps for stats.min_value / max_value / mean.
    int min_bin = 255;
    int max_bin = 0;
    uint64_t pixel_sum = 0;

    if (channel == HistogramChannel::Luminance) {
        for (int y = 0; y < height; ++y) {
            const unsigned char* row = data + static_cast<std::size_t>(y) * stride;
            for (int x = 0; x < width; ++x) {
                const int idx = x * 3;
                const unsigned char b = row[idx];
                const unsigned char g = row[idx + 1];
                const unsigned char r = row[idx + 2];
                const int lum = (static_cast<int>(r) * 77 + static_cast<int>(g) * 150 + static_cast<int>(b) * 29) >> 8;
                ++bins[lum];
                pixel_sum += static_cast<uint64_t>(lum);
                if (lum < min_bin) min_bin = lum;
                if (lum > max_bin) max_bin = lum;
            }
        }
    } else {
        const int channel_offset = (channel == HistogramChannel::Red)   ? 2
                                  : (channel == HistogramChannel::Green) ? 1
                                  :                                       0;
        for (int y = 0; y < height; ++y) {
            const unsigned char* row = data + static_cast<std::size_t>(y) * stride;
            for (int x = 0; x < width; ++x) {
                const int val = row[x * 3 + channel_offset];
                ++bins[val];
                pixel_sum += static_cast<uint64_t>(val);
                if (val < min_bin) min_bin = val;
                if (val > max_bin) max_bin = val;
            }
        }
    }

    result.total_pixels = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);

    result.max_count = *std::max_element(bins.begin(), bins.end());

    // Populate stats from values already tracked during the pixel pass.
    HistogramStats& stats = result.stats;
    if (result.total_pixels > 0) {
        stats.min_value = min_bin;
        stats.max_value = max_bin;
        stats.mean = static_cast<double>(pixel_sum) / static_cast<double>(result.total_pixels);
    }

    // Median still requires one cumulative pass over bins.
    const uint64_t median_target = (result.total_pixels + 1ULL) / 2ULL;
    uint64_t cumulative = 0;
    for (int i = 0; i < 256; ++i) {
        cumulative += bins[i];
        if (cumulative >= median_target) {
            stats.median = i;
            break;
        }
    }

    return result;
}
