#include "FluorescenceChannelAnalysis.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace {

unsigned char Luminance(const unsigned char* pixel)
{
    const int blue = pixel[0];
    const int green = pixel[1];
    const int red = pixel[2];
    return static_cast<unsigned char>((red * 77 + green * 150 + blue * 29) >> 8);
}

unsigned char Percentile(
    const std::array<std::uint64_t, 256>& histogram,
    std::uint64_t pixel_count,
    double fraction)
{
    if (pixel_count == 0) {
        return 0;
    }
    const double bounded = std::clamp(fraction, 0.0, 1.0);
    const std::uint64_t rank = static_cast<std::uint64_t>(
        std::floor(bounded * static_cast<double>(pixel_count - 1)));
    std::uint64_t cumulative = 0;
    for (std::size_t value = 0; value < histogram.size(); ++value) {
        cumulative += histogram[value];
        if (cumulative > rank) {
            return static_cast<unsigned char>(value);
        }
    }
    return 255;
}

void EnsureUsableRange(
    unsigned char minimum,
    unsigned char maximum,
    unsigned char& black_level,
    unsigned char& white_level)
{
    if (white_level > black_level) {
        return;
    }
    black_level = minimum;
    white_level = maximum;
    if (white_level > black_level) {
        return;
    }

    const int center = static_cast<int>(minimum);
    const int black = std::max(0, center - 1);
    const int white = std::min(255, center + 1);
    black_level = static_cast<unsigned char>(black);
    white_level = static_cast<unsigned char>(white);
    if (white_level <= black_level) {
        black_level = center >= 255 ? 254 : 0;
        white_level = center >= 255 ? 255 : 1;
    }
}

} // namespace

FluorescenceChannelStatistics FluorescenceChannelAnalysis::Analyze(
    const ImageFrame& frame,
    double low_percentile,
    double high_percentile)
{
    FluorescenceChannelStatistics result;
    if (!frame.IsValid()) {
        return result;
    }

    std::uint64_t weighted_sum = 0;
    for (int y = 0; y < frame.height; ++y) {
        const unsigned char* row = frame.bgr.data() +
            static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.stride);
        for (int x = 0; x < frame.width; ++x) {
            const unsigned char value = Luminance(row + x * 3);
            ++result.histogram[value];
            weighted_sum += value;
        }
    }

    result.pixel_count = static_cast<std::uint64_t>(frame.width) *
        static_cast<std::uint64_t>(frame.height);
    if (result.pixel_count == 0) {
        return result;
    }

    result.minimum = Percentile(result.histogram, result.pixel_count, 0.0);
    result.maximum = Percentile(result.histogram, result.pixel_count, 1.0);
    result.suggested_black_level = Percentile(
        result.histogram, result.pixel_count, low_percentile);
    result.suggested_white_level = Percentile(
        result.histogram, result.pixel_count, high_percentile);
    EnsureUsableRange(
        result.minimum,
        result.maximum,
        result.suggested_black_level,
        result.suggested_white_level);

    result.mean = static_cast<double>(weighted_sum) /
        static_cast<double>(result.pixel_count);
    result.clipped_fraction = static_cast<double>(result.histogram[255]) /
        static_cast<double>(result.pixel_count);

    if (result.clipped_fraction >= 0.005) {
        result.exposure = FluorescenceExposureState::Saturated;
    } else if (result.suggested_white_level <= 32 || result.mean <= 8.0) {
        result.exposure = FluorescenceExposureState::Underexposed;
    } else if (static_cast<int>(result.suggested_white_level) -
                   static_cast<int>(result.suggested_black_level) < 24) {
        result.exposure = FluorescenceExposureState::LowContrast;
    } else {
        result.exposure = FluorescenceExposureState::Balanced;
    }
    return result;
}

bool FluorescenceChannelAnalysis::ApplySuggestedLevels(
    FluorescenceChannel& channel,
    const FluorescenceChannelStatistics& statistics)
{
    if (!statistics.IsValid() ||
        statistics.suggested_white_level <= statistics.suggested_black_level) {
        return false;
    }
    channel.black_level = statistics.suggested_black_level;
    channel.white_level = statistics.suggested_white_level;
    return true;
}

std::wstring FluorescenceChannelAnalysis::ExposureLabel(FluorescenceExposureState state)
{
    switch (state) {
    case FluorescenceExposureState::Underexposed:
        return L"Underexposed";
    case FluorescenceExposureState::LowContrast:
        return L"Low contrast";
    case FluorescenceExposureState::Balanced:
        return L"Exposure OK";
    case FluorescenceExposureState::Saturated:
        return L"Saturated";
    case FluorescenceExposureState::NoData:
    default:
        return L"No image data";
    }
}
