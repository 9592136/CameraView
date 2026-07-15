#include "EdfProcessor.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>

namespace {

unsigned char LuminanceAt(const ImageFrame& frame, int x, int y)
{
    x = std::clamp(x, 0, frame.width - 1);
    y = std::clamp(y, 0, frame.height - 1);
    const unsigned char* pixel =
        frame.bgr.data() +
        static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.stride) +
        static_cast<std::size_t>(x) * 3U;
    const int blue = pixel[0];
    const int green = pixel[1];
    const int red = pixel[2];
    return static_cast<unsigned char>((red * 77 + green * 150 + blue * 29) >> 8);
}

int FocusScore(const ImageFrame& frame, int x, int y, int radius)
{
    const int center = LuminanceAt(frame, x, y);
    int score = 0;
    for (int offset = 1; offset <= radius; ++offset) {
        score += std::abs(center - LuminanceAt(frame, x - offset, y));
        score += std::abs(center - LuminanceAt(frame, x + offset, y));
        score += std::abs(center - LuminanceAt(frame, x, y - offset));
        score += std::abs(center - LuminanceAt(frame, x, y + offset));
    }
    return score;
}

const ImageFrame* FirstUsableFrame(const std::vector<ImageFrame>& frames)
{
    for (const ImageFrame& frame : frames) {
        if (frame.IsValid()) {
            return &frame;
        }
    }
    return nullptr;
}

bool IsCancelled(const std::atomic_bool* cancel_requested)
{
    return cancel_requested && cancel_requested->load();
}

void ReportProgress(const std::function<void(int)>& progress_callback, int percent)
{
    if (progress_callback) {
        progress_callback(std::clamp(percent, 0, 100));
    }
}

} // namespace

EdfResult EdfProcessor::ComposeFocusStack(
    const std::vector<ImageFrame>& frames,
    const EdfOptions& options,
    const std::atomic_bool* cancel_requested,
    const std::function<void(int)>& progress_callback)
{
    ReportProgress(progress_callback, 0);
    if (IsCancelled(cancel_requested)) {
        return {};
    }

    const ImageFrame* base = FirstUsableFrame(frames);
    if (!base) {
        return {};
    }

    const int focus_radius = std::clamp(options.focus_radius, 1, 16);

    EdfResult result;
    result.composite_frame.width = base->width;
    result.composite_frame.height = base->height;
    result.composite_frame.stride = (result.composite_frame.width * 3 + 3) & ~3;
    result.composite_frame.timestamp = base->timestamp;
    result.composite_frame.sequence = base->sequence;
    result.composite_frame.bgr.assign(
        static_cast<std::size_t>(result.composite_frame.stride) *
        static_cast<std::size_t>(result.composite_frame.height),
        0);

    result.focus_map.width = result.composite_frame.width;
    result.focus_map.height = result.composite_frame.height;
    result.focus_map.stride = result.composite_frame.stride;
    result.focus_map.timestamp = base->timestamp;
    result.focus_map.sequence = base->sequence;
    result.focus_map.bgr.assign(
        static_cast<std::size_t>(result.focus_map.stride) *
        static_cast<std::size_t>(result.focus_map.height),
        0);

    for (int y = 0; y < result.composite_frame.height; ++y) {
        if (IsCancelled(cancel_requested)) {
            return {};
        }
        unsigned char* dst =
            result.composite_frame.bgr.data() +
            static_cast<std::size_t>(y) * static_cast<std::size_t>(result.composite_frame.stride);
        unsigned char* focus_dst =
            result.focus_map.bgr.data() +
            static_cast<std::size_t>(y) * static_cast<std::size_t>(result.focus_map.stride);
        for (int x = 0; x < result.composite_frame.width; ++x) {
            const ImageFrame* best_frame = nullptr;
            std::size_t best_index = 0;
            int best_score = -1;
            for (std::size_t index = 0; index < frames.size(); ++index) {
                const ImageFrame& frame = frames[index];
                if (!frame.IsValid() ||
                    frame.width != result.composite_frame.width ||
                    frame.height != result.composite_frame.height) {
                    continue;
                }
                const int score = FocusScore(frame, x, y, focus_radius);
                if (score > best_score) {
                    best_score = score;
                    best_frame = &frame;
                    best_index = index;
                }
            }

            if (!best_frame) {
                continue;
            }
            const unsigned char* src =
                best_frame->bgr.data() +
                static_cast<std::size_t>(y) * static_cast<std::size_t>(best_frame->stride) +
                static_cast<std::size_t>(x) * 3U;
            dst[x * 3 + 0] = src[0];
            dst[x * 3 + 1] = src[1];
            dst[x * 3 + 2] = src[2];

            const unsigned char focus_value = frames.size() <= 1
                ? 0
                : static_cast<unsigned char>((best_index * 255U) / (frames.size() - 1U));
            focus_dst[x * 3 + 0] = focus_value;
            focus_dst[x * 3 + 1] = focus_value;
            focus_dst[x * 3 + 2] = focus_value;
        }
        ReportProgress(progress_callback, ((y + 1) * 100) / result.composite_frame.height);
    }

    ReportProgress(progress_callback, 100);
    return result;
}

EdfResult EdfProcessor::ComposeFocusStack(
    const std::vector<ImageFrame>& frames,
    const std::atomic_bool* cancel_requested,
    const std::function<void(int)>& progress_callback)
{
    return ComposeFocusStack(frames, EdfOptions{}, cancel_requested, progress_callback);
}

ImageFrame EdfProcessor::FuseFocusStack(
    const std::vector<ImageFrame>& frames,
    const EdfOptions& options,
    const std::atomic_bool* cancel_requested,
    const std::function<void(int)>& progress_callback)
{
    return ComposeFocusStack(frames, options, cancel_requested, progress_callback).composite_frame;
}

ImageFrame EdfProcessor::FuseFocusStack(
    const std::vector<ImageFrame>& frames,
    const std::atomic_bool* cancel_requested,
    const std::function<void(int)>& progress_callback)
{
    return ComposeFocusStack(frames, cancel_requested, progress_callback).composite_frame;
}
