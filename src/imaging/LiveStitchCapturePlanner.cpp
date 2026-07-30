#include "LiveStitchCapturePlanner.h"

#include "ImageRegistration.h"
#include "ProcessingParameterRules.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace {

constexpr double kMinimumRegistrationConfidence = 0.05;
constexpr double kFastMinimumRegistrationConfidence = 0.0;
constexpr double kMaximumRegistrationScore = 2.5;
constexpr double kFastMaximumRegistrationScore = 6.5;
constexpr int kFallbackCandidateCount = 4;
constexpr int kFastFallbackCandidateCount = 3;
constexpr int kFallbackSearchDivisor = 64;
constexpr int kFastFallbackSearchDivisor = 24;
constexpr int kMaxReferenceTileCount = 5;
constexpr int kLowOverlapWarningMarginPercent = 8;

struct ReferenceMatch {
    bool valid = false;
    std::size_t reference_index = 0;
    int dx = 0;
    int dy = 0;
    int tile_offset_x = 0;
    int tile_offset_y = 0;
    int movement_percent = 0;
    int overlap_percent = 0;
    double confidence = 0.0;
    double score = std::numeric_limits<double>::infinity();
};

bool HasReadablePixels(const ImageFrame& frame)
{
    return frame.IsValid() &&
        frame.stride >= frame.width * 3 &&
        frame.bgr.size() >=
            static_cast<std::size_t>(frame.stride) * static_cast<std::size_t>(frame.height);
}

int PreviewScaleFor(const ImageFrame& reference, const ImageFrame& candidate, int max_registration_edge)
{
    const int max_edge = std::max({
        reference.width,
        reference.height,
        candidate.width,
        candidate.height});
    const int capped_edge = std::max(128, max_registration_edge);
    return std::max(1, (std::max(1, max_edge) + capped_edge - 1) / capped_edge);
}

ImageFrame DownsampleAverage(const ImageFrame& source, int scale)
{
    if (!HasReadablePixels(source) || scale <= 1) {
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

int RoundedPercent(double value)
{
    return static_cast<int>(std::lround(value * 100.0));
}

int OverlapPercentFor(const ImageFrame& reference, const ImageFrame& candidate, int dx, int dy)
{
    const int left = std::max(0, dx);
    const int top = std::max(0, dy);
    const int right = std::min(reference.width, dx + candidate.width);
    const int bottom = std::min(reference.height, dy + candidate.height);
    const int width = std::max(0, right - left);
    const int height = std::max(0, bottom - top);
    const int min_area = std::max(1, std::min(
        reference.width * reference.height,
        candidate.width * candidate.height));
    return RoundedPercent(static_cast<double>(width) * static_cast<double>(height) / static_cast<double>(min_area));
}

int MovementPercentFor(const ImageFrame& reference, const ImageFrame& candidate, int dx, int dy)
{
    const int width = std::max(1, std::min(reference.width, candidate.width));
    const int height = std::max(1, std::min(reference.height, candidate.height));
    const double movement_x = static_cast<double>(std::abs(dx)) / static_cast<double>(width);
    const double movement_y = static_cast<double>(std::abs(dy)) / static_cast<double>(height);
    return RoundedPercent(std::max(movement_x, movement_y));
}

bool IsBetterTranslation(const TranslationOffset& candidate, const TranslationOffset& current)
{
    if (!candidate.valid) {
        return false;
    }
    if (!current.valid) {
        return true;
    }
    if (std::abs(candidate.score - current.score) <= 1e-9) {
        return candidate.confidence > current.confidence;
    }
    return candidate.score < current.score;
}

TranslationOffset BestTranslation(
    const ImageFrame& reference_preview,
    const ImageFrame& candidate_preview,
    int search_percent,
    bool fast_mode)
{
    const StitchSearchRadius search_radius =
        ProcessingParameterRules::RegistrationSearchRadius(
            reference_preview.width,
            reference_preview.height,
            search_percent);
    const int max_dx = std::min(reference_preview.width - 1, search_radius.x);
    const int max_dy = std::min(reference_preview.height - 1, search_radius.y);

    TranslationOffset translation =
        ImageRegistration::EstimateOrbTranslation(reference_preview, candidate_preview, max_dx, max_dy);
    if (translation.valid && translation.confidence >= kMinimumRegistrationConfidence) {
        return translation;
    }

    const int fallback_divisor = fast_mode ? kFastFallbackSearchDivisor : kFallbackSearchDivisor;
    const int step_x = std::max(1, reference_preview.width / fallback_divisor);
    const int step_y = std::max(1, reference_preview.height / fallback_divisor);
    const int candidate_count = fast_mode ? kFastFallbackCandidateCount : kFallbackCandidateCount;
    const std::vector<TranslationOffset> candidates =
        ImageRegistration::EstimateTranslationCandidates(
            reference_preview,
            candidate_preview,
            -max_dx,
            max_dx,
            -max_dy,
            max_dy,
            candidate_count,
            step_x,
            step_y);
    if (candidates.empty()) {
        return {};
    }

    const int refine_radius_x = fast_mode ? std::max(2, step_x) : std::max(1, step_x / 2);
    const int refine_radius_y = fast_mode ? std::max(2, step_y) : std::max(1, step_y / 2);
    TranslationOffset best;
    const int refinement_count = std::min(
        static_cast<int>(candidates.size()),
        fast_mode ? kFastFallbackCandidateCount : kFallbackCandidateCount);
    for (int index = 0; index < refinement_count; ++index) {
        const TranslationOffset& coarse = candidates[static_cast<std::size_t>(index)];
        TranslationOffset refined = ImageRegistration::RefineTranslation(
            reference_preview,
            candidate_preview,
            coarse.dx,
            coarse.dy,
            refine_radius_x,
            refine_radius_y);
        if (!refined.valid ||
            refined.dx < -max_dx || refined.dx > max_dx ||
            refined.dy < -max_dy || refined.dy > max_dy) {
            refined = coarse;
        }
        if (IsBetterTranslation(refined, best)) {
            best = refined;
        }
    }

    return best.valid ? best : candidates.front();
}

double MinimumConfidenceFor(const LiveStitchCaptureOptions& options)
{
    return options.fast_mode ? kFastMinimumRegistrationConfidence : kMinimumRegistrationConfidence;
}

double MaximumScoreFor(const LiveStitchCaptureOptions& options)
{
    return options.fast_mode ? kFastMaximumRegistrationScore : kMaximumRegistrationScore;
}

ReferenceMatch MatchReferenceTile(
    const std::vector<StitchTile>& tiles,
    std::size_t reference_index,
    const ImageFrame& candidate,
    const LiveStitchCaptureOptions& options)
{
    ReferenceMatch match;
    match.reference_index = reference_index;

    const StitchTile& reference = tiles[reference_index];
    const StitchTile& previous = tiles.back();
    if (!HasReadablePixels(reference.frame) || !HasReadablePixels(previous.frame)) {
        return match;
    }

    const int scale = PreviewScaleFor(reference.frame, candidate, options.max_registration_edge);
    const ImageFrame reference_preview = DownsampleAverage(reference.frame, scale);
    const ImageFrame candidate_preview = DownsampleAverage(candidate, scale);
    const TranslationOffset preview_translation =
        BestTranslation(reference_preview, candidate_preview, options.search_percent, options.fast_mode);
    if (!preview_translation.valid ||
        preview_translation.confidence < MinimumConfidenceFor(options) ||
        preview_translation.score > MaximumScoreFor(options)) {
        return match;
    }

    match.valid = true;
    match.dx = static_cast<int>(
        std::lround(static_cast<double>(preview_translation.dx) * static_cast<double>(scale)));
    match.dy = static_cast<int>(
        std::lround(static_cast<double>(preview_translation.dy) * static_cast<double>(scale)));
    match.tile_offset_x = reference.offset_x + match.dx;
    match.tile_offset_y = reference.offset_y + match.dy;
    match.confidence = preview_translation.confidence;
    match.score = preview_translation.score;
    match.movement_percent = MovementPercentFor(
        previous.frame,
        candidate,
        match.tile_offset_x - previous.offset_x,
        match.tile_offset_y - previous.offset_y);
    match.overlap_percent = OverlapPercentFor(reference.frame, candidate, match.dx, match.dy);
    return match;
}

bool IsBetterAcceptedMatch(const ReferenceMatch& candidate, const ReferenceMatch& current)
{
    if (!current.valid) {
        return true;
    }

    const double candidate_quality =
        candidate.score -
        candidate.confidence * 0.20 +
        static_cast<double>(candidate.overlap_percent) * -0.01;
    const double current_quality =
        current.score -
        current.confidence * 0.20 +
        static_cast<double>(current.overlap_percent) * -0.01;
    return candidate_quality < current_quality;
}

bool IsBetterDiagnosticMatch(const ReferenceMatch& candidate, const ReferenceMatch& current)
{
    if (!current.valid) {
        return true;
    }
    if (candidate.overlap_percent != current.overlap_percent) {
        return candidate.overlap_percent > current.overlap_percent;
    }
    return candidate.score < current.score;
}

void ApplyMatchToDecision(LiveStitchCaptureDecision& decision, const ReferenceMatch& match)
{
    decision.registration_valid = true;
    decision.dx = match.dx;
    decision.dy = match.dy;
    decision.tile_offset_x = match.tile_offset_x;
    decision.tile_offset_y = match.tile_offset_y;
    decision.reference_tile_index = match.reference_index;
    decision.confidence = match.confidence;
    decision.movement_percent = match.movement_percent;
    decision.overlap_percent = match.overlap_percent;
}

} // namespace

LiveStitchCaptureDecision LiveStitchCapturePlanner::Evaluate(
    const std::vector<StitchTile>& tiles,
    const ImageFrame& candidate,
    LiveStitchCaptureOptions options)
{
    LiveStitchCaptureDecision decision;
    options.min_overlap_percent = std::clamp(options.min_overlap_percent, 1, 95);
    options.min_movement_percent = std::clamp(options.min_movement_percent, 1, 95);
    options.search_percent = ProcessingParameterRules::ClampStitchSearchPercent(options.search_percent);
    options.reference_tile_count = std::clamp(options.reference_tile_count, 1, kMaxReferenceTileCount);

    if (!HasReadablePixels(candidate)) {
        decision.message = L"No camera frame available for live stitch.";
        return decision;
    }
    if (tiles.empty()) {
        decision.should_capture = true;
        decision.first_tile = true;
        decision.overlap_percent = 100;
        decision.reference_tile_index = 0;
        decision.message = L"Live stitch first tile captured.";
        return decision;
    }

    const StitchTile& previous = tiles.back();
    if (!HasReadablePixels(previous.frame)) {
        decision.message = L"Live stitch skipped: previous tile is not readable.";
        return decision;
    }

    ReferenceMatch last_match;
    ReferenceMatch accepted_match;
    ReferenceMatch diagnostic_match;
    const std::size_t references =
        std::min(static_cast<std::size_t>(options.reference_tile_count), tiles.size());
    for (std::size_t recency = 0; recency < references; ++recency) {
        const std::size_t reference_index = tiles.size() - 1U - recency;
        const ReferenceMatch match = MatchReferenceTile(tiles, reference_index, candidate, options);
        if (recency == 0) {
            last_match = match;
        }
        if (!match.valid) {
            continue;
        }
        if (IsBetterDiagnosticMatch(match, diagnostic_match)) {
            diagnostic_match = match;
        }
        if (match.movement_percent >= options.min_movement_percent &&
            match.overlap_percent >= options.min_overlap_percent &&
            IsBetterAcceptedMatch(match, accepted_match)) {
            accepted_match = match;
        }
    }

    if (last_match.valid &&
        last_match.movement_percent < options.min_movement_percent) {
        ApplyMatchToDecision(decision, last_match);
        decision.message =
            L"Live stitch waiting: movement " +
            std::to_wstring(decision.movement_percent) +
            L"% is below " +
            std::to_wstring(options.min_movement_percent) +
            L"%.";
        return decision;
    }

    if (!accepted_match.valid) {
        if (diagnostic_match.valid) {
            ApplyMatchToDecision(decision, diagnostic_match);
            if (decision.overlap_percent < options.min_overlap_percent) {
                const int confirmed_low_overlap_threshold =
                    std::max(1, options.min_overlap_percent - kLowOverlapWarningMarginPercent);
                decision.out_of_range_warning =
                    decision.overlap_percent < confirmed_low_overlap_threshold;
                decision.message =
                    decision.out_of_range_warning
                        ? L"Live stitch warning: overlap " +
                            std::to_wstring(decision.overlap_percent) +
                            L"% is below " +
                            std::to_wstring(options.min_overlap_percent) +
                            L"%. Move back toward the last tile."
                        : L"Live stitch waiting: overlap " +
                            std::to_wstring(decision.overlap_percent) +
                            L"% is near the " +
                            std::to_wstring(options.min_overlap_percent) +
                            L"% limit. Keep the fields overlapping.";
                return decision;
            }

            decision.message =
                L"Live stitch waiting: movement " +
                std::to_wstring(decision.movement_percent) +
                L"% is below " +
                std::to_wstring(options.min_movement_percent) +
                L"%.";
            return decision;
        }

        decision.match_missing = true;
        decision.message =
            L"Live stitch registration uncertain: no reliable overlap match. Move slowly and keep the fields overlapping.";
        return decision;
    }

    ApplyMatchToDecision(decision, accepted_match);
    decision.should_capture = true;
    decision.message =
        L"Live stitch tile accepted: movement " +
        std::to_wstring(decision.movement_percent) +
        L"%, overlap " +
        std::to_wstring(decision.overlap_percent) +
        L"%.";
    return decision;
}
