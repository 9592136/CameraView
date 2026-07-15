#include "ImageRegistration.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace {

unsigned char LuminanceAt(const ImageFrame& frame, int x, int y)
{
    const unsigned char* pixel =
        frame.bgr.data() +
        static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.stride) +
        static_cast<std::size_t>(x) * 3U;
    const int blue = pixel[0];
    const int green = pixel[1];
    const int red = pixel[2];
    return static_cast<unsigned char>((red * 77 + green * 150 + blue * 29) >> 8);
}

bool OverlapForOffset(
    const ImageFrame& reference,
    const ImageFrame& moving,
    int dx,
    int dy,
    int& ref_x0,
    int& ref_y0,
    int& moving_x0,
    int& moving_y0,
    int& width,
    int& height)
{
    const int left = std::max(0, dx);
    const int top = std::max(0, dy);
    const int right = std::min(reference.width, dx + moving.width);
    const int bottom = std::min(reference.height, dy + moving.height);
    width = right - left;
    height = bottom - top;
    if (width <= 0 || height <= 0) {
        return false;
    }

    ref_x0 = left;
    ref_y0 = top;
    moving_x0 = left - dx;
    moving_y0 = top - dy;
    return true;
}

TranslationOffset EstimateTranslationInRange(
    const ImageFrame& reference,
    const ImageFrame& moving,
    int min_dx,
    int max_dx,
    int min_dy,
    int max_dy)
{
    if (!reference.IsValid() || !moving.IsValid() || min_dx > max_dx || min_dy > max_dy) {
        return {};
    }

    TranslationOffset best;
    double best_error = std::numeric_limits<double>::infinity();
    int best_overlap = 0;
    const int min_overlap_pixels = std::max(16, std::min(reference.width * reference.height, moving.width * moving.height) / 16);

    for (int dy = min_dy; dy <= max_dy; ++dy) {
        for (int dx = min_dx; dx <= max_dx; ++dx) {
            int ref_x0 = 0;
            int ref_y0 = 0;
            int moving_x0 = 0;
            int moving_y0 = 0;
            int width = 0;
            int height = 0;
            if (!OverlapForOffset(reference, moving, dx, dy, ref_x0, ref_y0, moving_x0, moving_y0, width, height)) {
                continue;
            }

            const int overlap_pixels = width * height;
            if (overlap_pixels < min_overlap_pixels) {
                continue;
            }

            long long error_sum = 0;
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    const int ref_value = LuminanceAt(reference, ref_x0 + x, ref_y0 + y);
                    const int moving_value = LuminanceAt(moving, moving_x0 + x, moving_y0 + y);
                    error_sum += std::abs(ref_value - moving_value);
                }
            }

            const double error = static_cast<double>(error_sum) / static_cast<double>(overlap_pixels);
            if (error < best_error ||
                (error == best_error && overlap_pixels > best_overlap)) {
                best_error = error;
                best_overlap = overlap_pixels;
                best.dx = dx;
                best.dy = dy;
                best.score = error;
                best.valid = true;
            }
        }
    }

    return best;
}

} // namespace

TranslationOffset ImageRegistration::EstimateTranslation(
    const ImageFrame& reference,
    const ImageFrame& moving,
    int max_shift_x,
    int max_shift_y)
{
    if (!reference.IsValid() || !moving.IsValid() || max_shift_x < 0 || max_shift_y < 0) {
        return {};
    }

    return EstimateTranslationInRange(
        reference,
        moving,
        -max_shift_x,
        max_shift_x,
        -max_shift_y,
        max_shift_y);
}

TranslationOffset ImageRegistration::RefineTranslation(
    const ImageFrame& reference,
    const ImageFrame& moving,
    int initial_dx,
    int initial_dy,
    int search_radius_x,
    int search_radius_y)
{
    if (!reference.IsValid() || !moving.IsValid() || search_radius_x < 0 || search_radius_y < 0) {
        return {};
    }

    return EstimateTranslationInRange(
        reference,
        moving,
        initial_dx - search_radius_x,
        initial_dx + search_radius_x,
        initial_dy - search_radius_y,
        initial_dy + search_radius_y);
}
