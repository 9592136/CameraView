#include "imaging/ImageFilterProcessor.h"

#include <algorithm>
#include <iostream>

namespace {

int fail(const char* message)
{
    std::cerr << message << '\n';
    return 1;
}

ImageFrame makeFrame(int width, int height, unsigned char value = 0)
{
    ImageFrame frame;
    frame.width = width;
    frame.height = height;
    frame.stride = width * 3;
    frame.sequence = 42;
    frame.timestamp = 17;
    frame.bgr.assign(static_cast<std::size_t>(frame.stride) * height, value);
    return frame;
}

unsigned char& channel(ImageFrame& frame, int x, int y, int index)
{
    return frame.bgr[static_cast<std::size_t>(y) * frame.stride + x * 3 + index];
}

unsigned char channel(const ImageFrame& frame, int x, int y, int index)
{
    return frame.bgr[static_cast<std::size_t>(y) * frame.stride + x * 3 + index];
}

bool sameGeometry(const ImageFrame& lhs, const ImageFrame& rhs)
{
    return lhs.width == rhs.width && lhs.height == rhs.height && lhs.stride == rhs.stride &&
        lhs.sequence == rhs.sequence && lhs.timestamp == rhs.timestamp;
}

} // namespace

int main()
{
    ImageFrame color = makeFrame(3, 2);
    channel(color, 0, 0, 0) = 10;
    channel(color, 0, 0, 1) = 80;
    channel(color, 0, 0, 2) = 220;
    color.bgr[3] = 30;
    color.bgr[4] = 120;
    color.bgr[5] = 200;

    const ImageFrame gray = ImageFilterProcessor::Apply(color, {ImageFilterKind::Grayscale, 0});
    if (!sameGeometry(color, gray) || channel(gray, 0, 0, 0) != channel(gray, 0, 0, 1) ||
        channel(gray, 0, 0, 1) != channel(gray, 0, 0, 2)) {
        return fail("Grayscale filtering did not preserve geometry or produce neutral pixels.");
    }

    const ImageFrame inverted = ImageFilterProcessor::Apply(color, {ImageFilterKind::Invert, 0});
    if (channel(inverted, 0, 0, 0) != 245 || channel(inverted, 0, 0, 1) != 175 ||
        channel(inverted, 0, 0, 2) != 35) {
        return fail("Invert filtering produced incorrect channel values.");
    }

    ImageFrame contrast = makeFrame(2, 1);
    for (int component = 0; component < 3; ++component) {
        channel(contrast, 0, 0, component) = 40;
        channel(contrast, 1, 0, component) = 200;
    }
    const ImageFrame stretched = ImageFilterProcessor::Apply(
        contrast, {ImageFilterKind::AutoContrast, 0});
    if (channel(stretched, 0, 0, 0) != 0 || channel(stretched, 1, 0, 0) != 255) {
        return fail("Auto contrast did not stretch the available intensity range.");
    }

    ImageFrame levels = makeFrame(16, 1);
    for (int x = 0; x < levels.width; ++x) {
        const unsigned char value = static_cast<unsigned char>(80 + x * 4);
        for (int component = 0; component < 3; ++component) channel(levels, x, 0, component) = value;
    }
    const ImageFrame equalized = ImageFilterProcessor::Apply(
        levels, {ImageFilterKind::HistogramEqualization, 0});
    if (equalized.bgr == levels.bgr || channel(equalized, 0, 0, 0) > channel(equalized, 15, 0, 0)) {
        return fail("Histogram equalization did not redistribute intensity values.");
    }

    ImageFrame impulse = makeFrame(9, 9);
    for (int component = 0; component < 3; ++component) channel(impulse, 4, 4, component) = 255;
    const ImageFrame blurred = ImageFilterProcessor::Apply(
        impulse, {ImageFilterKind::GaussianBlur, 2});
    if (channel(blurred, 4, 4, 0) >= 255 || channel(blurred, 4, 3, 0) == 0) {
        return fail("Blur filtering did not distribute an impulse to neighboring pixels.");
    }

    ImageFrame noisy = makeFrame(7, 7, 20);
    for (int component = 0; component < 3; ++component) channel(noisy, 3, 3, component) = 255;
    const ImageFrame denoised = ImageFilterProcessor::Apply(
        noisy, {ImageFilterKind::MedianDenoise, 1});
    if (channel(denoised, 3, 3, 0) != 20) {
        return fail("Median denoising did not remove an isolated impulse.");
    }

    ImageFrame soft_edge = makeFrame(9, 9, 80);
    for (int y = 0; y < soft_edge.height; ++y) {
        for (int x = 5; x < soft_edge.width; ++x) {
            for (int component = 0; component < 3; ++component) channel(soft_edge, x, y, component) = 160;
        }
    }
    const ImageFrame sharpened = ImageFilterProcessor::Apply(
        soft_edge, {ImageFilterKind::Sharpen, 100});
    if (sharpened.bgr == soft_edge.bgr) {
        return fail("Sharpen filtering did not enhance a soft intensity transition.");
    }

    const ImageFrame edge_map = ImageFilterProcessor::Apply(
        soft_edge, {ImageFilterKind::EdgeDetection, 0});
    if (*std::max_element(edge_map.bgr.begin(), edge_map.bgr.end()) == 0 ||
        channel(edge_map, 4, 4, 0) != channel(edge_map, 4, 4, 1)) {
        return fail("Edge detection did not produce a neutral edge response.");
    }

    const ImageFrame binary = ImageFilterProcessor::Apply(
        soft_edge, {ImageFilterKind::BinaryThreshold, 120});
    if (channel(binary, 2, 2, 0) != 0 || channel(binary, 7, 2, 0) != 255) {
        return fail("Binary thresholding did not classify dark and bright regions.");
    }

    const std::vector<ImageFilterStep> pipeline{
        {ImageFilterKind::Grayscale, 0}, {ImageFilterKind::Invert, 0}};
    const ImageFrame pipelined = ImageFilterProcessor::ApplyPipeline(color, pipeline);
    const ImageFrame manual = ImageFilterProcessor::Apply(
        ImageFilterProcessor::Apply(color, pipeline[0]), pipeline[1]);
    if (pipelined.bgr != manual.bgr) {
        return fail("Image filter pipeline order differs from sequential application.");
    }

    if (ImageFilterProcessor::Apply({}, {ImageFilterKind::Invert, 0}).IsValid() ||
        ImageFilterProcessor::ApplyPipeline({}, pipeline).IsValid()) {
        return fail("Image filters accepted an invalid source frame.");
    }
    return 0;
}
