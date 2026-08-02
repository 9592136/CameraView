#pragma once

#include "../domain/Geometry.h"
#include "../domain/ImageFrame.h"

#include <vector>

enum class ImageProfileChannel {
    Luminance,
    Red,
    Green,
    Blue
};

struct ImageProfileSample {
    double distance_pixels = 0.0;
    double intensity = 0.0;
};

struct ImageProfileResult {
    ImagePoint first;
    ImagePoint second;
    ImageProfileChannel channel = ImageProfileChannel::Luminance;
    double pixel_length = 0.0;
    double min_intensity = 0.0;
    double max_intensity = 0.0;
    double mean_intensity = 0.0;
    double standard_deviation = 0.0;
    std::vector<ImageProfileSample> samples;

    bool IsValid() const { return pixel_length > 0.0 && samples.size() >= 2; }
};

class ImageProfileSampler {
public:
    static ImageProfileResult Sample(
        const ImageFrame& frame,
        ImagePoint first,
        ImagePoint second,
        ImageProfileChannel channel = ImageProfileChannel::Luminance,
        int maximum_samples = 4096);
};
