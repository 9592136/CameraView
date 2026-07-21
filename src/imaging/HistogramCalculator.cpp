#include "HistogramCalculator.h"

#include <algorithm>

HistogramData ComputeHistogram(const ImageFrame& frame, HistogramChannel channel)
{
    HistogramData result;
    if (!frame.IsValid()) {
        return result;
    }

    const int width = frame.width;
    const int height = frame.height;
    const int stride = frame.stride;
    const unsigned char* data = frame.bgr.data();

    auto& bins = result.bins;

    if (channel == HistogramChannel::Luminance) {
        for (int y = 0; y < height; ++y) {
            const unsigned char* row = data + static_cast<std::size_t>(y) * stride;
            for (int x = 0; x < width; ++x) {
                const int idx = x * 3;
                const unsigned char b = row[idx];
                const unsigned char g = row[idx + 1];
                const unsigned char r = row[idx + 2];
                // ITU-R BT.601 luminance
                const int lum = (static_cast<int>(r) * 77 + static_cast<int>(g) * 150 + static_cast<int>(b) * 29) >> 8;
                ++bins[lum];
            }
        }
    } else if (channel == HistogramChannel::Red) {
        for (int y = 0; y < height; ++y) {
            const unsigned char* row = data + static_cast<std::size_t>(y) * stride;
            for (int x = 0; x < width; ++x) {
                ++bins[row[x * 3 + 2]];
            }
        }
    } else if (channel == HistogramChannel::Green) {
        for (int y = 0; y < height; ++y) {
            const unsigned char* row = data + static_cast<std::size_t>(y) * stride;
            for (int x = 0; x < width; ++x) {
                ++bins[row[x * 3 + 1]];
            }
        }
    } else if (channel == HistogramChannel::Blue) {
        for (int y = 0; y < height; ++y) {
            const unsigned char* row = data + static_cast<std::size_t>(y) * stride;
            for (int x = 0; x < width; ++x) {
                ++bins[row[x * 3]];
            }
        }
    }

    result.max_count = *std::max_element(bins.begin(), bins.end());
    result.total_pixels = static_cast<unsigned long long>(width) * static_cast<unsigned long long>(height);

    // Compute summary statistics in a single pass
    auto& s = result.stats;
    // Min
    for (int i = 0; i < 256; ++i) {
        if (bins[i] > 0) { s.min_value = i; break; }
    }
    // Max
    for (int i = 255; i >= 0; --i) {
        if (bins[i] > 0) { s.max_value = i; break; }
    }
    // Mean
    unsigned long long sum = 0;
    for (int i = 0; i < 256; ++i) {
        sum += bins[i] * static_cast<unsigned long long>(i);
    }
    s.mean = static_cast<double>(sum) / static_cast<double>(result.total_pixels);
    // Median
    unsigned long long half = result.total_pixels / 2;
    unsigned long long cum = 0;
    for (int i = 0; i < 256; ++i) {
        cum += bins[i];
        if (cum >= half) { s.median = i; break; }
    }

    return result;
}
