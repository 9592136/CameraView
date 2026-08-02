#include "imaging/SmartTargetCounter.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace {

ImageFrame makeSyntheticTargets()
{
    ImageFrame frame;
    frame.width = 240;
    frame.height = 180;
    frame.stride = frame.width * 3;
    frame.bgr.assign(static_cast<std::size_t>(frame.stride) * frame.height, 28);
    const std::vector<std::pair<int, int>> centers{
        {35, 35}, {100, 35}, {175, 38}, {55, 105}, {130, 110}, {205, 120}};
    for (const auto& [center_x, center_y] : centers) {
        for (int y = center_y - 9; y <= center_y + 9; ++y) {
            for (int x = center_x - 9; x <= center_x + 9; ++x) {
                const double distance = std::hypot(x - center_x, y - center_y);
                if (distance > 8.0) continue;
                const unsigned char value = distance < 5.0 ? 225 : 160;
                const std::size_t offset = static_cast<std::size_t>(y) * frame.stride + x * 3;
                frame.bgr[offset] = value;
                frame.bgr[offset + 1] = value;
                frame.bgr[offset + 2] = value;
            }
        }
    }
    // Bright non-target structures make sure simple intensity thresholding is
    // not mistaken for target similarity.
    for (int y = 65; y < 83; ++y) {
        for (int x = 185; x < 203; ++x) {
            if (x < 190 || x > 197 || y < 70 || y > 77) {
                const std::size_t offset = static_cast<std::size_t>(y) * frame.stride + x * 3;
                frame.bgr[offset] = frame.bgr[offset + 1] = frame.bgr[offset + 2] = 220;
            }
        }
    }
    return frame;
}

ImageFrame makeIrregularTargets()
{
    ImageFrame frame;
    frame.width = 220;
    frame.height = 150;
    frame.stride = frame.width * 3;
    frame.bgr.assign(static_cast<std::size_t>(frame.stride) * frame.height, 24);
    const std::vector<std::pair<int, int>> origins{
        {20, 20}, {90, 20}, {45, 90}, {150, 85}};
    for (const auto& [origin_x, origin_y] : origins) {
        for (int y = 2; y <= 21; ++y) {
            for (int x = 2; x <= 17; ++x) {
                const bool vertical_stroke = x <= 6;
                const bool bottom_stroke = y >= 17;
                const bool marker = x >= 13 && y <= 6;
                if (!vertical_stroke && !bottom_stroke && !marker) continue;
                const unsigned char value = marker ? 245 : 185;
                const std::size_t offset = static_cast<std::size_t>(origin_y + y) * frame.stride +
                    (origin_x + x) * 3;
                frame.bgr[offset] = value;
                frame.bgr[offset + 1] = value;
                frame.bgr[offset + 2] = value;
            }
        }
    }
    return frame;
}

int fail(const char* message)
{
    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main()
{
    if (!SmartTargetCounter::IsAvailable()) return fail("OpenCV smart counter backend is unavailable.");

    const ImageFrame frame = makeSyntheticTargets();
    SmartTargetCountOptions options;
    options.similarity_threshold = 0.72;
    options.scale_tolerance = 0.12;
    options.scale_steps = 5;
    std::vector<int> progress_updates;
    const SmartTargetCountResult result = SmartTargetCounter::Count(
        frame, {{25, 25, 20, 20}, {120, 100, 20, 20}}, options, nullptr,
        [&progress_updates](int value) { progress_updates.push_back(value); });
    if (!result.succeeded || result.canceled || result.matches.size() != 6 ||
        result.evaluated_template_count != 10) {
        std::cerr << "Expected 6 targets, got " << result.matches.size() << '\n';
        return fail("Multi-sample smart target counting produced the wrong result.");
    }
    for (const SmartTargetMatch& match : result.matches) {
        if (match.confidence < options.similarity_threshold || match.region.width < 15 || match.region.height < 15) {
            return fail("Smart target match metadata is invalid.");
        }
    }
    const std::vector<std::pair<int, int>> expected_centers{
        {35, 35}, {100, 35}, {175, 38}, {55, 105}, {130, 110}, {205, 120}};
    for (const auto& [expected_x, expected_y] : expected_centers) {
        const bool found = std::any_of(result.matches.begin(), result.matches.end(),
            [expected_x, expected_y](const SmartTargetMatch& match) {
                const double center_x = match.region.x + match.region.width / 2.0;
                const double center_y = match.region.y + match.region.height / 2.0;
                return std::hypot(center_x - expected_x, center_y - expected_y) <= 4.0;
            });
        if (!found) return fail("Smart target counting missed an expected target location.");
    }
    if (progress_updates.empty() || progress_updates.front() != 0 || progress_updates.back() != 100 ||
        !std::is_sorted(progress_updates.begin(), progress_updates.end())) {
        return fail("Smart target counting reported incomplete or non-monotonic progress.");
    }

    const SmartTargetCountResult single_sample_result = SmartTargetCounter::Count(
        frame, {{25, 25, 20, 20}}, options);
    if (!single_sample_result.succeeded || single_sample_result.matches.size() != expected_centers.size()) {
        return fail("Single-sample smart target counting produced the wrong result.");
    }

    SmartTargetCountOptions irregular_options;
    irregular_options.similarity_threshold = 0.78;
    irregular_options.scale_tolerance = 0.0;
    const SmartTargetCountResult irregular_result = SmartTargetCounter::Count(
        makeIrregularTargets(), {{20, 20, 20, 24}}, irregular_options);
    if (!irregular_result.succeeded || irregular_result.matches.size() != 4) {
        std::cerr << "Expected 4 irregular targets, got " << irregular_result.matches.size() << '\n';
        return fail("Rectangular smart-count samples did not recognize irregular targets.");
    }

    std::atomic_bool canceled{true};
    const SmartTargetCountResult canceled_result = SmartTargetCounter::Count(
        frame, {{25, 25, 20, 20}}, options, &canceled);
    if (!canceled_result.canceled || canceled_result.succeeded) {
        return fail("Smart target counting did not honor cancellation.");
    }

    const SmartTargetCountResult empty_result = SmartTargetCounter::Count(frame, {}, options);
    if (empty_result.succeeded || empty_result.message.empty()) {
        return fail("Smart target counting accepted an empty sample set.");
    }
    return 0;
}
