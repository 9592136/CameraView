#include "imaging/ImageProfileSampler.h"

#include <cmath>
#include <iostream>

namespace {

int fail(const char* message)
{
    std::cerr << message << '\n';
    return 1;
}

bool near(double left, double right, double tolerance = 1e-6)
{
    return std::abs(left - right) <= tolerance;
}

ImageFrame gradientFrame()
{
    ImageFrame frame;
    frame.width = 11;
    frame.height = 2;
    frame.stride = 36;
    frame.bgr.assign(static_cast<std::size_t>(frame.stride * frame.height), 0);
    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            unsigned char* pixel = frame.bgr.data() + y * frame.stride + x * 3;
            pixel[0] = static_cast<unsigned char>(x * 20);
            pixel[1] = static_cast<unsigned char>(x * 20);
            pixel[2] = static_cast<unsigned char>(x * 20);
        }
    }
    return frame;
}

} // namespace

int main()
{
    const ImageFrame frame = gradientFrame();
    const ImageProfileResult profile = ImageProfileSampler::Sample(
        frame, {0.0, 0.0}, {10.0, 0.0}, ImageProfileChannel::Luminance);
    if (!profile.IsValid() || profile.samples.size() != 11 ||
        !near(profile.pixel_length, 10.0) ||
        !near(profile.min_intensity, 0.0) || !near(profile.max_intensity, 200.0) ||
        !near(profile.mean_intensity, 100.0) ||
        !near(profile.standard_deviation, std::sqrt(4000.0))) {
        return fail("Horizontal gradient profile or statistics are incorrect.");
    }
    for (std::size_t index = 0; index < profile.samples.size(); ++index) {
        if (!near(profile.samples[index].distance_pixels, static_cast<double>(index)) ||
            !near(profile.samples[index].intensity, static_cast<double>(index * 20))) {
            return fail("Profile sample coordinates are incorrect.");
        }
    }

    ImageFrame colors;
    colors.width = 2;
    colors.height = 1;
    colors.stride = 8;
    colors.bgr = {10, 20, 30, 50, 60, 70, 0, 0};
    const ImageProfileResult red = ImageProfileSampler::Sample(
        colors, {0.0, 0.0}, {1.0, 0.0}, ImageProfileChannel::Red);
    const ImageProfileResult green = ImageProfileSampler::Sample(
        colors, {0.0, 0.0}, {1.0, 0.0}, ImageProfileChannel::Green);
    const ImageProfileResult blue = ImageProfileSampler::Sample(
        colors, {0.0, 0.0}, {1.0, 0.0}, ImageProfileChannel::Blue);
    if (!near(red.samples.front().intensity, 30.0) || !near(red.samples.back().intensity, 70.0) ||
        !near(green.samples.front().intensity, 20.0) || !near(green.samples.back().intensity, 60.0) ||
        !near(blue.samples.front().intensity, 10.0) || !near(blue.samples.back().intensity, 50.0)) {
        return fail("RGB channel profiles are incorrect.");
    }

    if (ImageProfileSampler::Sample(frame, {2.0, 1.0}, {2.0, 1.0}).IsValid()) {
        return fail("Zero-length profile was accepted.");
    }
    return 0;
}
