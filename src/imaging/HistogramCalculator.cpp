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

    HistogramStats& stats = result.stats;
    for (int i = 0; i < 256; ++i) {
        if (bins[i] > 0) {
            stats.min_value = i;
            break;
        }
    }
    for (int i = 255; i >= 0; --i) {
        if (bins[i] > 0) {
            stats.max_value = i;
            break;
        }
    }

    unsigned long long sum = 0;
    for (int i = 0; i < 256; ++i) {
        sum += bins[i] * static_cast<unsigned long long>(i);
    }
    stats.mean = static_cast<double>(sum) / static_cast<double>(result.total_pixels);

    const unsigned long long median_target = (result.total_pixels + 1ULL) / 2ULL;
    unsigned long long cumulative = 0;
    for (int i = 0; i < 256; ++i) {
        cumulative += bins[i];
        if (cumulative >= median_target) {
            stats.median = i;
            break;
        }
    }

    return result;
}
