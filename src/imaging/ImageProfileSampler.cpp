#include "ImageProfileSampler.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace {

double ChannelValue(const unsigned char* pixel, ImageProfileChannel channel)
{
    const double blue = pixel[0];
    const double green = pixel[1];
    const double red = pixel[2];
    switch (channel) {
    case ImageProfileChannel::Red:
        return red;
    case ImageProfileChannel::Green:
        return green;
    case ImageProfileChannel::Blue:
        return blue;
    case ImageProfileChannel::Luminance:
    default:
        return 0.114 * blue + 0.587 * green + 0.299 * red;
    }
}

double BilinearIntensity(
    const ImageFrame& frame,
    double x,
    double y,
    ImageProfileChannel channel)
{
    x = std::clamp(x, 0.0, static_cast<double>(frame.width - 1));
    y = std::clamp(y, 0.0, static_cast<double>(frame.height - 1));
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(frame.width - 1, x0 + 1);
    const int y1 = std::min(frame.height - 1, y0 + 1);
    const double tx = x - x0;
    const double ty = y - y0;
    auto value = [&](int px, int py) {
        const unsigned char* pixel = frame.bgr.data() +
            static_cast<std::size_t>(py) * static_cast<std::size_t>(frame.stride) +
            static_cast<std::size_t>(px) * 3U;
        return ChannelValue(pixel, channel);
    };
    const double top = value(x0, y0) * (1.0 - tx) + value(x1, y0) * tx;
    const double bottom = value(x0, y1) * (1.0 - tx) + value(x1, y1) * tx;
    return top * (1.0 - ty) + bottom * ty;
}

} // namespace

ImageProfileResult ImageProfileSampler::Sample(
    const ImageFrame& frame,
    ImagePoint first,
    ImagePoint second,
    ImageProfileChannel channel,
    int maximum_samples)
{
    ImageProfileResult result;
    result.first = first;
    result.second = second;
    result.channel = channel;
    if (!frame.IsValid() || frame.width <= 0 || frame.height <= 0 ||
        frame.stride < frame.width * 3 || maximum_samples < 2) {
        return result;
    }

    const double dx = second.x - first.x;
    const double dy = second.y - first.y;
    result.pixel_length = std::hypot(dx, dy);
    if (!std::isfinite(result.pixel_length) || result.pixel_length < 1e-6) {
        result.pixel_length = 0.0;
        return result;
    }

    const int sample_count = std::clamp(
        static_cast<int>(std::ceil(result.pixel_length)) + 1,
        2,
        maximum_samples);
    result.samples.reserve(static_cast<std::size_t>(sample_count));
    result.min_intensity = std::numeric_limits<double>::max();
    result.max_intensity = std::numeric_limits<double>::lowest();
    double sum = 0.0;
    for (int index = 0; index < sample_count; ++index) {
        const double t = sample_count == 1 ? 0.0 :
            static_cast<double>(index) / static_cast<double>(sample_count - 1);
        const double intensity = BilinearIntensity(
            frame, first.x + dx * t, first.y + dy * t, channel);
        result.samples.push_back({result.pixel_length * t, intensity});
        result.min_intensity = std::min(result.min_intensity, intensity);
        result.max_intensity = std::max(result.max_intensity, intensity);
        sum += intensity;
    }
    result.mean_intensity = sum / result.samples.size();
    double squared_sum = 0.0;
    for (const ImageProfileSample& sample : result.samples) {
        const double delta = sample.intensity - result.mean_intensity;
        squared_sum += delta * delta;
    }
    result.standard_deviation = std::sqrt(squared_sum / result.samples.size());
    return result;
}
