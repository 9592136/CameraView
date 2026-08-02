#include "ImageFilterProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#ifdef CAMERAVIEW_WITH_OPENCV
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#endif

namespace {

bool HasReadablePixels(const ImageFrame& frame)
{
    return frame.IsValid() && frame.stride >= frame.width * 3 &&
        frame.bgr.size() >= static_cast<std::size_t>(frame.stride) * frame.height;
}

unsigned char clampByte(double value)
{
    return static_cast<unsigned char>(std::clamp(static_cast<int>(std::lround(value)), 0, 255));
}

unsigned char luminance(const unsigned char* pixel)
{
    return clampByte(pixel[0] * 0.114 + pixel[1] * 0.587 + pixel[2] * 0.299);
}

ImageFrame makeOutput(const ImageFrame& source)
{
    ImageFrame output = source;
    output.bgr.resize(static_cast<std::size_t>(source.stride) * source.height);
    return output;
}

ImageFrame grayscale(const ImageFrame& source)
{
    ImageFrame output = makeOutput(source);
    for (int y = 0; y < source.height; ++y) {
        const unsigned char* input = source.bgr.data() + static_cast<std::size_t>(y) * source.stride;
        unsigned char* target = output.bgr.data() + static_cast<std::size_t>(y) * output.stride;
        for (int x = 0; x < source.width; ++x) {
            const unsigned char value = luminance(input + x * 3);
            target[x * 3] = target[x * 3 + 1] = target[x * 3 + 2] = value;
        }
    }
    return output;
}

ImageFrame invert(const ImageFrame& source)
{
    ImageFrame output = makeOutput(source);
    for (int y = 0; y < source.height; ++y) {
        const unsigned char* input = source.bgr.data() + static_cast<std::size_t>(y) * source.stride;
        unsigned char* target = output.bgr.data() + static_cast<std::size_t>(y) * output.stride;
        for (int x = 0; x < source.width * 3; ++x) target[x] = static_cast<unsigned char>(255 - input[x]);
    }
    return output;
}

ImageFrame autoContrast(const ImageFrame& source)
{
    std::array<int, 3> minimum{255, 255, 255};
    std::array<int, 3> maximum{0, 0, 0};
    for (int y = 0; y < source.height; ++y) {
        const unsigned char* row = source.bgr.data() + static_cast<std::size_t>(y) * source.stride;
        for (int x = 0; x < source.width; ++x) {
            for (int channel = 0; channel < 3; ++channel) {
                minimum[channel] = std::min(minimum[channel], static_cast<int>(row[x * 3 + channel]));
                maximum[channel] = std::max(maximum[channel], static_cast<int>(row[x * 3 + channel]));
            }
        }
    }
    ImageFrame output = makeOutput(source);
    for (int y = 0; y < source.height; ++y) {
        const unsigned char* input = source.bgr.data() + static_cast<std::size_t>(y) * source.stride;
        unsigned char* target = output.bgr.data() + static_cast<std::size_t>(y) * output.stride;
        for (int x = 0; x < source.width; ++x) {
            for (int channel = 0; channel < 3; ++channel) {
                const int range = maximum[channel] - minimum[channel];
                target[x * 3 + channel] = range == 0 ? input[x * 3 + channel] :
                    clampByte((input[x * 3 + channel] - minimum[channel]) * 255.0 / range);
            }
        }
    }
    return output;
}

ImageFrame equalizeHistogram(const ImageFrame& source)
{
    ImageFrame output = makeOutput(source);
    std::array<std::array<int, 256>, 3> histograms{};
    for (int y = 0; y < source.height; ++y) {
        const unsigned char* row = source.bgr.data() + static_cast<std::size_t>(y) * source.stride;
        for (int x = 0; x < source.width; ++x) {
            for (int channel = 0; channel < 3; ++channel) ++histograms[channel][row[x * 3 + channel]];
        }
    }
    std::array<std::array<unsigned char, 256>, 3> lookups{};
    const int pixel_count = source.width * source.height;
    for (int channel = 0; channel < 3; ++channel) {
        int cumulative = 0;
        int first_nonzero = 0;
        while (first_nonzero < 256 && histograms[channel][first_nonzero] == 0) ++first_nonzero;
        const int baseline = first_nonzero < 256 ? histograms[channel][first_nonzero] : 0;
        for (int value = 0; value < 256; ++value) {
            cumulative += histograms[channel][value];
            const int denominator = pixel_count - baseline;
            lookups[channel][value] = denominator <= 0 ? static_cast<unsigned char>(value) :
                clampByte((cumulative - baseline) * 255.0 / denominator);
        }
    }
    for (int y = 0; y < source.height; ++y) {
        const unsigned char* input = source.bgr.data() + static_cast<std::size_t>(y) * source.stride;
        unsigned char* target = output.bgr.data() + static_cast<std::size_t>(y) * output.stride;
        for (int x = 0; x < source.width; ++x) {
            for (int channel = 0; channel < 3; ++channel) {
                target[x * 3 + channel] = lookups[channel][input[x * 3 + channel]];
            }
        }
    }
    return output;
}

ImageFrame boxBlur(const ImageFrame& source, int radius)
{
    radius = std::clamp(radius, 1, 10);
    const int width = source.width;
    const int height = source.height;
    std::vector<double> horizontal(static_cast<std::size_t>(width) * height * 3);
    for (int y = 0; y < height; ++y) {
        const unsigned char* row = source.bgr.data() + static_cast<std::size_t>(y) * source.stride;
        for (int channel = 0; channel < 3; ++channel) {
            int sum = 0;
            for (int x = 0; x <= std::min(radius, width - 1); ++x) sum += row[x * 3 + channel];
            for (int x = 0; x < width; ++x) {
                const int left = std::max(0, x - radius);
                const int right = std::min(width - 1, x + radius);
                if (x > 0) {
                    const int removed = x - radius - 1;
                    const int added = x + radius;
                    if (removed >= 0) sum -= row[removed * 3 + channel];
                    if (added < width) sum += row[added * 3 + channel];
                }
                horizontal[(static_cast<std::size_t>(y) * width + x) * 3 + channel] =
                    sum / static_cast<double>(right - left + 1);
            }
        }
    }
    ImageFrame output = makeOutput(source);
    for (int x = 0; x < width; ++x) {
        for (int channel = 0; channel < 3; ++channel) {
            double sum = 0.0;
            for (int y = 0; y <= std::min(radius, height - 1); ++y) {
                sum += horizontal[(static_cast<std::size_t>(y) * width + x) * 3 + channel];
            }
            for (int y = 0; y < height; ++y) {
                const int top = std::max(0, y - radius);
                const int bottom = std::min(height - 1, y + radius);
                if (y > 0) {
                    const int removed = y - radius - 1;
                    const int added = y + radius;
                    if (removed >= 0) sum -= horizontal[(static_cast<std::size_t>(removed) * width + x) * 3 + channel];
                    if (added < height) sum += horizontal[(static_cast<std::size_t>(added) * width + x) * 3 + channel];
                }
                output.bgr[static_cast<std::size_t>(y) * output.stride + x * 3 + channel] =
                    clampByte(sum / (bottom - top + 1));
            }
        }
    }
    return output;
}

ImageFrame medianDenoise(const ImageFrame& source, int radius)
{
    radius = std::clamp(radius, 1, 3);
    ImageFrame output = makeOutput(source);
    std::vector<unsigned char> values;
    values.reserve(static_cast<std::size_t>((radius * 2 + 1) * (radius * 2 + 1)));
    for (int y = 0; y < source.height; ++y) {
        for (int x = 0; x < source.width; ++x) {
            for (int channel = 0; channel < 3; ++channel) {
                values.clear();
                for (int sample_y = std::max(0, y - radius); sample_y <= std::min(source.height - 1, y + radius); ++sample_y) {
                    const unsigned char* row = source.bgr.data() + static_cast<std::size_t>(sample_y) * source.stride;
                    for (int sample_x = std::max(0, x - radius); sample_x <= std::min(source.width - 1, x + radius); ++sample_x) {
                        values.push_back(row[sample_x * 3 + channel]);
                    }
                }
                const auto middle = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2);
                std::nth_element(values.begin(), middle, values.end());
                output.bgr[static_cast<std::size_t>(y) * output.stride + x * 3 + channel] = *middle;
            }
        }
    }
    return output;
}

ImageFrame sharpen(const ImageFrame& source, int percent)
{
    const ImageFrame blurred = boxBlur(source, 1);
    const double amount = std::clamp(percent, 10, 300) / 100.0;
    ImageFrame output = makeOutput(source);
    for (int y = 0; y < source.height; ++y) {
        const unsigned char* input = source.bgr.data() + static_cast<std::size_t>(y) * source.stride;
        const unsigned char* smooth = blurred.bgr.data() + static_cast<std::size_t>(y) * blurred.stride;
        unsigned char* target = output.bgr.data() + static_cast<std::size_t>(y) * output.stride;
        for (int x = 0; x < source.width * 3; ++x) {
            target[x] = clampByte(input[x] + amount * (input[x] - smooth[x]));
        }
    }
    return output;
}

ImageFrame edges(const ImageFrame& source, int threshold)
{
    ImageFrame output = makeOutput(source);
    std::fill(output.bgr.begin(), output.bgr.end(), 0);
    threshold = std::clamp(threshold, 0, 255);
    const std::array<int, 9> kernel_x{-1, 0, 1, -2, 0, 2, -1, 0, 1};
    const std::array<int, 9> kernel_y{-1, -2, -1, 0, 0, 0, 1, 2, 1};
    for (int y = 1; y < source.height - 1; ++y) {
        for (int x = 1; x < source.width - 1; ++x) {
            int gradient_x = 0;
            int gradient_y = 0;
            int index = 0;
            for (int offset_y = -1; offset_y <= 1; ++offset_y) {
                const unsigned char* row = source.bgr.data() +
                    static_cast<std::size_t>(y + offset_y) * source.stride;
                for (int offset_x = -1; offset_x <= 1; ++offset_x, ++index) {
                    const int value = luminance(row + (x + offset_x) * 3);
                    gradient_x += value * kernel_x[index];
                    gradient_y += value * kernel_y[index];
                }
            }
            int value = std::clamp(static_cast<int>(std::hypot(gradient_x, gradient_y)), 0, 255);
            if (threshold > 0) value = value >= threshold ? 255 : 0;
            unsigned char* pixel = output.bgr.data() + static_cast<std::size_t>(y) * output.stride + x * 3;
            pixel[0] = pixel[1] = pixel[2] = static_cast<unsigned char>(value);
        }
    }
    return output;
}

ImageFrame binaryThreshold(const ImageFrame& source, int threshold)
{
    ImageFrame output = makeOutput(source);
    threshold = std::clamp(threshold, 0, 255);
    for (int y = 0; y < source.height; ++y) {
        const unsigned char* input = source.bgr.data() + static_cast<std::size_t>(y) * source.stride;
        unsigned char* target = output.bgr.data() + static_cast<std::size_t>(y) * output.stride;
        for (int x = 0; x < source.width; ++x) {
            const unsigned char value = luminance(input + x * 3) >= threshold ? 255 : 0;
            target[x * 3] = target[x * 3 + 1] = target[x * 3 + 2] = value;
        }
    }
    return output;
}

#ifdef CAMERAVIEW_WITH_OPENCV

ImageFrame fromMat(const ImageFrame& source, const cv::Mat& image)
{
    ImageFrame output = makeOutput(source);
    for (int y = 0; y < source.height; ++y) {
        std::memcpy(output.bgr.data() + static_cast<std::size_t>(y) * output.stride,
            image.ptr(y), static_cast<std::size_t>(source.width) * 3);
    }
    return output;
}

ImageFrame applyOpenCv(const ImageFrame& source, ImageFilterStep step)
{
    cv::Mat input(source.height, source.width, CV_8UC3,
        const_cast<unsigned char*>(source.bgr.data()), static_cast<std::size_t>(source.stride));
    cv::Mat output;
    switch (step.kind) {
    case ImageFilterKind::Grayscale: {
        cv::Mat gray;
        cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
        cv::cvtColor(gray, output, cv::COLOR_GRAY2BGR);
        break;
    }
    case ImageFilterKind::Invert:
        cv::bitwise_not(input, output);
        break;
    case ImageFilterKind::AutoContrast: {
        std::vector<cv::Mat> channels;
        cv::split(input, channels);
        for (cv::Mat& channel : channels) {
            double minimum = 0.0;
            double maximum = 0.0;
            cv::minMaxLoc(channel, &minimum, &maximum);
            if (maximum > minimum) channel.convertTo(channel, CV_8U,
                255.0 / (maximum - minimum), -minimum * 255.0 / (maximum - minimum));
        }
        cv::merge(channels, output);
        break;
    }
    case ImageFilterKind::HistogramEqualization: {
        cv::Mat ycrcb;
        cv::cvtColor(input, ycrcb, cv::COLOR_BGR2YCrCb);
        std::vector<cv::Mat> channels;
        cv::split(ycrcb, channels);
        cv::equalizeHist(channels[0], channels[0]);
        cv::merge(channels, ycrcb);
        cv::cvtColor(ycrcb, output, cv::COLOR_YCrCb2BGR);
        break;
    }
    case ImageFilterKind::GaussianBlur: {
        const int radius = std::clamp(step.parameter, 1, 10);
        cv::GaussianBlur(input, output, {radius * 2 + 1, radius * 2 + 1}, 0.0);
        break;
    }
    case ImageFilterKind::MedianDenoise: {
        const int radius = std::clamp(step.parameter, 1, 3);
        cv::medianBlur(input, output, radius * 2 + 1);
        break;
    }
    case ImageFilterKind::Sharpen: {
        cv::Mat smooth;
        cv::GaussianBlur(input, smooth, {0, 0}, 1.0);
        const double amount = std::clamp(step.parameter, 10, 300) / 100.0;
        cv::addWeighted(input, 1.0 + amount, smooth, -amount, 0.0, output);
        break;
    }
    case ImageFilterKind::EdgeDetection: {
        cv::Mat gray;
        cv::Mat gradient_x;
        cv::Mat gradient_y;
        cv::Mat magnitude;
        cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
        cv::Sobel(gray, gradient_x, CV_32F, 1, 0, 3);
        cv::Sobel(gray, gradient_y, CV_32F, 0, 1, 3);
        cv::magnitude(gradient_x, gradient_y, magnitude);
        cv::normalize(magnitude, magnitude, 0.0, 255.0, cv::NORM_MINMAX, CV_8U);
        const int threshold = std::clamp(step.parameter, 0, 255);
        if (threshold > 0) cv::threshold(magnitude, magnitude, threshold, 255, cv::THRESH_BINARY);
        cv::cvtColor(magnitude, output, cv::COLOR_GRAY2BGR);
        break;
    }
    case ImageFilterKind::BinaryThreshold: {
        cv::Mat gray;
        cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
        cv::threshold(gray, gray, std::clamp(step.parameter, 0, 255), 255, cv::THRESH_BINARY);
        cv::cvtColor(gray, output, cv::COLOR_GRAY2BGR);
        break;
    }
    }
    return fromMat(source, output);
}

#endif

ImageFrame applyFallback(const ImageFrame& source, ImageFilterStep step)
{
    switch (step.kind) {
    case ImageFilterKind::Grayscale: return grayscale(source);
    case ImageFilterKind::Invert: return invert(source);
    case ImageFilterKind::AutoContrast: return autoContrast(source);
    case ImageFilterKind::HistogramEqualization: return equalizeHistogram(source);
    case ImageFilterKind::GaussianBlur: return boxBlur(source, step.parameter);
    case ImageFilterKind::MedianDenoise: return medianDenoise(source, step.parameter);
    case ImageFilterKind::Sharpen: return sharpen(source, step.parameter);
    case ImageFilterKind::EdgeDetection: return edges(source, step.parameter);
    case ImageFilterKind::BinaryThreshold: return binaryThreshold(source, step.parameter);
    }
    return source;
}

} // namespace

ImageFrame ImageFilterProcessor::Apply(const ImageFrame& source, ImageFilterStep step)
{
    if (!HasReadablePixels(source)) return {};
#ifdef CAMERAVIEW_WITH_OPENCV
    try {
        return applyOpenCv(source, step);
    } catch (const cv::Exception&) {
        return applyFallback(source, step);
    }
#else
    return applyFallback(source, step);
#endif
}

ImageFrame ImageFilterProcessor::ApplyPipeline(
    const ImageFrame& source,
    const std::vector<ImageFilterStep>& steps)
{
    if (!HasReadablePixels(source)) return {};
    ImageFrame output = source;
    for (const ImageFilterStep& step : steps) output = Apply(output, step);
    return output;
}
