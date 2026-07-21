#pragma once

#include "../domain/ImageFrame.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

/// Channel selection for histogram computation.
enum class HistogramChannel : int
{
    Luminance = 0, ///< Weighted luminance: 0.299*R + 0.587*G + 0.114*B
    Red,
    Green,
    Blue
};

/// Statistical summary computed from histogram bins.
struct HistogramStats
{
    int min_value = 0;      ///< Smallest non-zero bin index
    int max_value = 255;    ///< Largest non-zero bin index
    double mean = 0.0;      ///< Weighted mean
    int median = 0;         ///< Bin where cumulative >= 50%
};

/// Result of a histogram computation.
struct HistogramData
{
    static constexpr int kBinCount = 256;
    std::array<unsigned long long, kBinCount> bins{};
    unsigned long long total_pixels = 0;
    unsigned long long max_count = 0;   ///< Peak bin count (for normalisation)
    HistogramStats stats;               ///< Derived summary statistics
};

/// Labels for UI channel selector.
inline const wchar_t* HistogramChannelLabel(HistogramChannel channel)
{
    switch (channel) {
    case HistogramChannel::Luminance:
        return L"Luminance";
    case HistogramChannel::Red:
        return L"Red";
    case HistogramChannel::Green:
        return L"Green";
    case HistogramChannel::Blue:
        return L"Blue";
    }
    return L"Unknown";
}

/// Computes a 256-bin intensity histogram from an ImageFrame.
HistogramData ComputeHistogram(const ImageFrame& frame, HistogramChannel channel);

/// Derives summary statistics from an already-computed histogram.
inline HistogramStats ComputeHistogramStats(const HistogramData& data)
{
    HistogramStats s;
    if (data.total_pixels == 0) return s;

    // Min (first non-zero bin)
    for (int i = 0; i < 256; ++i) {
        if (data.bins[i] > 0) { s.min_value = i; break; }
    }
    // Max (last non-zero bin)
    for (int i = 255; i >= 0; --i) {
        if (data.bins[i] > 0) { s.max_value = i; break; }
    }

    // Mean
    unsigned long long sum = 0;
    for (int i = 0; i < 256; ++i) {
        sum += data.bins[i] * static_cast<unsigned long long>(i);
    }
    s.mean = static_cast<double>(sum) / static_cast<double>(data.total_pixels);

    // Median (bin where cumulative passes 50%)
    unsigned long long half = data.total_pixels / 2;
    unsigned long long cumulative = 0;
    for (int i = 0; i < 256; ++i) {
        cumulative += data.bins[i];
        if (cumulative >= half) { s.median = i; break; }
    }

    return s;
}
