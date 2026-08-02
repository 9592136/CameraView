#pragma once

#include "../domain/ImageFrame.h"

#include <vector>

enum class ImageFilterKind {
    Grayscale,
    Invert,
    AutoContrast,
    HistogramEqualization,
    GaussianBlur,
    MedianDenoise,
    Sharpen,
    EdgeDetection,
    BinaryThreshold
};

struct ImageFilterStep {
    ImageFilterKind kind = ImageFilterKind::Grayscale;
    int parameter = 0;
};

class ImageFilterProcessor {
public:
    static ImageFrame Apply(const ImageFrame& source, ImageFilterStep step);
    static ImageFrame ApplyPipeline(
        const ImageFrame& source,
        const std::vector<ImageFilterStep>& steps);
};
