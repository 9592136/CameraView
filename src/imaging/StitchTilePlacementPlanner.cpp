#include "StitchTilePlacementPlanner.h"

#include "ProcessingParameterRules.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace {

constexpr int kMaxInteractiveRegistrationEdge = 160;
constexpr int kMaxExactAddTileRegistrationEdge = 160;
constexpr int kMaxInteractiveRegistrationRadius = 144;
constexpr int kMaxInteractiveBandCandidates = 3;
constexpr int kMaxInteractivePreviewCandidates = 6;
constexpr int kMaxInteractiveRefinementCandidates = 3;
constexpr int kMaxInteractiveRefinementScale = 4;
constexpr int kMaxInteractiveRefinementRadius = 6;
constexpr int kInteractivePreviewOffsetStep = 2;
constexpr double kInteractiveDriftFraction = 0.25;
constexpr double kConfidentInteractiveRegistrationScore = 2.5;
constexpr double kMinimumInteractiveRegistrationConfidence = 0.005;

enum AdjacentDirection {
    kAdjacentRight = 1,
    kAdjacentLeft = 2,
    kAdjacentDown = 4,
    kAdjacentUp = 8,
};

int RegistrationPreviewScale(const ImageFrame& reference, const ImageFrame& moving)
{
    const int max_edge = std::max({
        reference.width,
        reference.height,
        moving.width,
        moving.height});
    return std::max(1, (max_edge + kMaxInteractiveRegistrationEdge - 1) / kMaxInteractiveRegistrationEdge);
}

bool HasReadablePixels(const ImageFrame& frame)
{
    return frame.IsValid() &&
        frame.stride >= frame.width * 3 &&
        frame.bgr.size() >= static_cast<std::size_t>(frame.stride) * static_cast<std::size_t>(frame.height);
}

bool IsUsableTranslation(const TranslationOffset& translation)
{
    return translation.valid &&
        translation.confidence >= kMinimumInteractiveRegistrationConfidence;
}

bool IsBetterTranslation(const TranslationOffset& candidate, const TranslationOffset& current)
{
    if (!IsUsableTranslation(candidate)) {
        return false;
    }
    if (!IsUsableTranslation(current)) {
        return true;
    }
    if (std::abs(candidate.score - current.score) <= 1e-9) {
        return candidate.confidence > current.confidence;
    }
    return candidate.score < current.score;
}

void AppendCandidates(
    std::vector<TranslationOffset>& output,
    const ImageFrame& reference,
    const ImageFrame& moving,
    int min_dx,
    int max_dx,
    int min_dy,
    int max_dy)
{
    if (min_dx > max_dx || min_dy > max_dy) {
        return;
    }

    std::vector<TranslationOffset> candidates = ImageRegistration::EstimateTranslationCandidates(
        reference,
        moving,
        min_dx,
        max_dx,
        min_dy,
        max_dy,
        kMaxInteractiveBandCandidates,
        kInteractivePreviewOffsetStep,
        kInteractivePreviewOffsetStep);
    output.insert(output.end(), candidates.begin(), candidates.end());
}

std::vector<TranslationOffset> EstimateAdjacentPreviewCandidates(
    const ImageFrame& reference,
    const ImageFrame& moving,
    StitchSearchRadius search_radius,
    int directions)
{
    const int radius_x = std::min(search_radius.x, kMaxInteractiveRegistrationRadius);
    const int radius_y = std::min(search_radius.y, kMaxInteractiveRegistrationRadius);
    const int horizontal_drift =
        std::max(1, std::min(radius_y, static_cast<int>(std::ceil(reference.height * kInteractiveDriftFraction))));
    const int vertical_drift =
        std::max(1, std::min(radius_x, static_cast<int>(std::ceil(reference.width * kInteractiveDriftFraction))));

    std::vector<TranslationOffset> candidates;
    if (directions & kAdjacentRight) {
        AppendCandidates(candidates, reference, moving, 1, radius_x, -horizontal_drift, horizontal_drift);
    }
    if (directions & kAdjacentLeft) {
        AppendCandidates(candidates, reference, moving, -radius_x, -1, -horizontal_drift, horizontal_drift);
    }
    if (directions & kAdjacentDown) {
        AppendCandidates(candidates, reference, moving, -vertical_drift, vertical_drift, 1, radius_y);
    }
    if (directions & kAdjacentUp) {
        AppendCandidates(candidates, reference, moving, -vertical_drift, vertical_drift, -radius_y, -1);
    }

    if (candidates.empty() && directions != (kAdjacentRight | kAdjacentLeft | kAdjacentDown | kAdjacentUp)) {
        return EstimateAdjacentPreviewCandidates(
            reference,
            moving,
            search_radius,
            kAdjacentRight | kAdjacentLeft | kAdjacentDown | kAdjacentUp);
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const TranslationOffset& left, const TranslationOffset& right) {
            return left.score < right.score;
        });

    std::vector<TranslationOffset> unique_candidates;
    for (const TranslationOffset& candidate : candidates) {
        if (!IsUsableTranslation(candidate)) {
            continue;
        }

        const bool duplicate = std::any_of(
            unique_candidates.begin(),
            unique_candidates.end(),
            [&](const TranslationOffset& existing) {
                return std::abs(existing.dx - candidate.dx) <= 1 &&
                    std::abs(existing.dy - candidate.dy) <= 1;
            });
        if (!duplicate) {
            unique_candidates.push_back(candidate);
        }
        if (static_cast<int>(unique_candidates.size()) >= kMaxInteractivePreviewCandidates) {
            break;
        }
    }
    return unique_candidates;
}

int DirectionsForNextTile(const std::vector<StitchTile>& existing_tiles)
{
    if (existing_tiles.size() < 2) {
        return kAdjacentRight | kAdjacentDown;
    }

    const StitchTile& previous = existing_tiles[existing_tiles.size() - 1U];
    const StitchTile& before_previous = existing_tiles[existing_tiles.size() - 2U];
    const int dx = previous.offset_x - before_previous.offset_x;
    const int dy = previous.offset_y - before_previous.offset_y;
    if (std::abs(dx) >= std::abs(dy) && dx != 0) {
        return (dx > 0 ? kAdjacentRight : kAdjacentLeft) | kAdjacentDown;
    }
    if (dy != 0) {
        return (dy > 0 ? kAdjacentDown : kAdjacentUp) | kAdjacentLeft | kAdjacentRight;
    }
    return kAdjacentRight | kAdjacentDown;
}

bool ShouldUseExactAddTileRegistration(const ImageFrame& reference, const ImageFrame& moving)
{
    return std::max({
        reference.width,
        reference.height,
        moving.width,
        moving.height}) <= kMaxExactAddTileRegistrationEdge;
}

void PlaceAdjacentEstimate(StitchTilePlacementResult& result, const std::vector<StitchTile>& existing_tiles, int search_percent)
{
    const StitchTile& previous = existing_tiles.back();
    if (existing_tiles.size() >= 2) {
        const StitchTile& before_previous = existing_tiles[existing_tiles.size() - 2U];
        const int previous_dx = previous.offset_x - before_previous.offset_x;
        const int previous_dy = previous.offset_y - before_previous.offset_y;
        if (previous_dx != 0 || previous_dy != 0) {
            result.tile.offset_x = previous.offset_x + previous_dx;
            result.tile.offset_y = previous.offset_y + previous_dy;
            return;
        }
    }

    const StitchSearchRadius nominal_step =
        ProcessingParameterRules::RegistrationSearchRadius(
            previous.frame.width,
            previous.frame.height,
            search_percent);
    result.tile.offset_x = previous.offset_x + std::max(1, std::min(previous.frame.width, nominal_step.x));
    result.tile.offset_y = previous.offset_y;
}

ImageFrame DownsampleAverage(const ImageFrame& source, int scale)
{
    if (!source.IsValid() || scale <= 1) {
        return source;
    }

    ImageFrame preview;
    preview.width = std::max(1, (source.width + scale - 1) / scale);
    preview.height = std::max(1, (source.height + scale - 1) / scale);
    preview.stride = (preview.width * 3 + 3) & ~3;
    preview.timestamp = source.timestamp;
    preview.sequence = source.sequence;
    preview.bgr.assign(
        static_cast<std::size_t>(preview.stride) * static_cast<std::size_t>(preview.height),
        0);

    for (int y = 0; y < preview.height; ++y) {
        const int source_y0 = y * scale;
        const int source_y1 = std::min(source.height, source_y0 + scale);
        unsigned char* preview_row =
            preview.bgr.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(preview.stride);
        for (int x = 0; x < preview.width; ++x) {
            const int source_x0 = x * scale;
            const int source_x1 = std::min(source.width, source_x0 + scale);
            int blue_sum = 0;
            int green_sum = 0;
            int red_sum = 0;
            int samples = 0;
            for (int source_y = source_y0; source_y < source_y1; ++source_y) {
                const unsigned char* source_row =
                    source.bgr.data() + static_cast<std::size_t>(source_y) * static_cast<std::size_t>(source.stride);
                for (int source_x = source_x0; source_x < source_x1; ++source_x) {
                    const unsigned char* source_pixel = source_row + static_cast<std::size_t>(source_x) * 3U;
                    blue_sum += source_pixel[0];
                    green_sum += source_pixel[1];
                    red_sum += source_pixel[2];
                    ++samples;
                }
            }
            unsigned char* preview_pixel = preview_row + static_cast<std::size_t>(x) * 3U;
            preview_pixel[0] = static_cast<unsigned char>(blue_sum / std::max(1, samples));
            preview_pixel[1] = static_cast<unsigned char>(green_sum / std::max(1, samples));
            preview_pixel[2] = static_cast<unsigned char>(red_sum / std::max(1, samples));
        }
    }

    return preview;
}

TranslationOffset EstimateInteractiveTranslation(
    const ImageFrame& reference,
    const ImageFrame& moving,
    StitchSearchRadius search_radius,
    int directions)
{
    if (!HasReadablePixels(reference) || !HasReadablePixels(moving)) {
        return {};
    }

    const int scale = RegistrationPreviewScale(reference, moving);
    if (scale <= 1) {
        return ImageRegistration::EstimateTranslation(
            reference,
            moving,
            search_radius.x,
            search_radius.y);
    }

    const ImageFrame reference_preview = DownsampleAverage(reference, scale);
    const ImageFrame moving_preview = DownsampleAverage(moving, scale);
    const int preview_radius_x = std::clamp(
        static_cast<int>(std::ceil(static_cast<double>(search_radius.x) / static_cast<double>(scale))),
        1,
        kMaxInteractiveRegistrationRadius);
    const int preview_radius_y = std::clamp(
        static_cast<int>(std::ceil(static_cast<double>(search_radius.y) / static_cast<double>(scale))),
        1,
        kMaxInteractiveRegistrationRadius);
    const std::vector<TranslationOffset> preview_candidates = EstimateAdjacentPreviewCandidates(
        reference_preview,
        moving_preview,
        StitchSearchRadius{preview_radius_x, preview_radius_y},
        directions);
    if (preview_candidates.empty()) {
        return {};
    }

    TranslationOffset fallback_translation = preview_candidates.front();
    fallback_translation.dx *= scale;
    fallback_translation.dy *= scale;
    if (scale > kMaxInteractiveRefinementScale) {
        return fallback_translation;
    }

    TranslationOffset best_refined_translation;
    auto consider_refinement = [&](int initial_dx, int initial_dy) {
        const TranslationOffset refined_translation = ImageRegistration::RefineTranslation(
            reference,
            moving,
            initial_dx,
            initial_dy,
            std::max(2, std::min(kMaxInteractiveRefinementRadius, scale + 1)),
            std::max(2, std::min(kMaxInteractiveRefinementRadius, scale + 1)));
        if (IsBetterTranslation(refined_translation, best_refined_translation)) {
            best_refined_translation = refined_translation;
        }
    };

    const int refinement_count = std::min(
        static_cast<int>(preview_candidates.size()),
        kMaxInteractiveRefinementCandidates);
    for (int index = 0; index < refinement_count; ++index) {
        const TranslationOffset& preview_candidate = preview_candidates[static_cast<std::size_t>(index)];
        consider_refinement(preview_candidate.dx * scale, preview_candidate.dy * scale);
    }
    if (IsUsableTranslation(best_refined_translation) &&
        best_refined_translation.score <= kConfidentInteractiveRegistrationScore) {
        return best_refined_translation;
    }

    if (IsUsableTranslation(best_refined_translation)) {
        return best_refined_translation;
    }
    return IsUsableTranslation(fallback_translation) ? fallback_translation : TranslationOffset{};
}

} // namespace

StitchTilePlacementResult StitchTilePlacementPlanner::PlaceNext(
    ImageFrame frame,
    const std::vector<StitchTile>& existing_tiles,
    int search_percent)
{
    StitchTilePlacementResult result;
    result.tile.frame = std::move(frame);

    if (existing_tiles.empty()) {
        return result;
    }

    const StitchTile& previous = existing_tiles.back();
    if (!ShouldUseExactAddTileRegistration(previous.frame, result.tile.frame)) {
        PlaceAdjacentEstimate(result, existing_tiles, search_percent);
        result.tile.estimated_position = true;
        result.estimated = true;
        return result;
    }

    const StitchSearchRadius search_radius =
        ProcessingParameterRules::RegistrationSearchRadius(
            previous.frame.width,
            previous.frame.height,
            search_percent);
    const TranslationOffset registration = EstimateInteractiveTranslation(
        previous.frame,
        result.tile.frame,
        search_radius,
        DirectionsForNextTile(existing_tiles));

    if (IsUsableTranslation(registration)) {
        result.registration = registration;
        result.tile.offset_x = previous.offset_x + registration.dx;
        result.tile.offset_y = previous.offset_y + registration.dy;
        result.registered = true;
        return result;
    }

    result.tile.offset_x = previous.offset_x + previous.frame.width;
    result.tile.offset_y = previous.offset_y;
    return result;
}
