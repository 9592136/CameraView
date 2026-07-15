#include "ChannelFusionEngine.h"

#include <algorithm>
#include <cstddef>

namespace {

unsigned char Luminance(const unsigned char* pixel)
{
    const int blue = pixel[0];
    const int green = pixel[1];
    const int red = pixel[2];
    return static_cast<unsigned char>((red * 77 + green * 150 + blue * 29) >> 8);
}

unsigned char ScaleIntensity(unsigned char value, unsigned char black_level, unsigned char white_level)
{
    if (white_level <= black_level) {
        return value >= white_level ? 255 : 0;
    }
    if (value <= black_level) {
        return 0;
    }
    if (value >= white_level) {
        return 255;
    }

    const int range = static_cast<int>(white_level) - static_cast<int>(black_level);
    const int adjusted = static_cast<int>(value) - static_cast<int>(black_level);
    return static_cast<unsigned char>((adjusted * 255 + range / 2) / range);
}

unsigned char AddClamped(unsigned char current, unsigned char intensity, unsigned char color)
{
    const int contribution = (static_cast<int>(intensity) * static_cast<int>(color) + 127) / 255;
    return static_cast<unsigned char>(std::min(255, static_cast<int>(current) + contribution));
}

const FluorescenceChannel* FirstUsableChannel(const std::vector<FluorescenceChannel>& channels)
{
    for (const FluorescenceChannel& channel : channels) {
        if (channel.visible && channel.frame.IsValid()) {
            return &channel;
        }
    }
    return nullptr;
}

} // namespace

ImageFrame ChannelFusionEngine::Fuse(const std::vector<FluorescenceChannel>& channels)
{
    const FluorescenceChannel* base_channel = FirstUsableChannel(channels);
    if (!base_channel) {
        return {};
    }

    ImageFrame output;
    output.width = base_channel->frame.width;
    output.height = base_channel->frame.height;
    output.stride = (output.width * 3 + 3) & ~3;
    output.timestamp = base_channel->frame.timestamp;
    output.sequence = base_channel->frame.sequence;
    output.bgr.assign(static_cast<std::size_t>(output.stride) * static_cast<std::size_t>(output.height), 0);

    for (const FluorescenceChannel& channel : channels) {
        if (!channel.visible || !channel.frame.IsValid()) {
            continue;
        }

        const int width = std::min(output.width, channel.frame.width);
        const int height = std::min(output.height, channel.frame.height);
        for (int y = 0; y < height; ++y) {
            const unsigned char* src = channel.frame.bgr.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(channel.frame.stride);
            unsigned char* dst = output.bgr.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(output.stride);
            for (int x = 0; x < width; ++x) {
                const unsigned char value = ScaleIntensity(
                    Luminance(src + x * 3),
                    channel.black_level,
                    channel.white_level);
                dst[x * 3 + 0] = AddClamped(dst[x * 3 + 0], value, channel.color.b);
                dst[x * 3 + 1] = AddClamped(dst[x * 3 + 1], value, channel.color.g);
                dst[x * 3 + 2] = AddClamped(dst[x * 3 + 2], value, channel.color.r);
            }
        }
    }

    return output;
}
