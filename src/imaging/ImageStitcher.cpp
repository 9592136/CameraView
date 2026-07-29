#include "ImageStitcher.h"

#include "ImageRegistration.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace {

struct Bounds {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

struct TileConstraint {
    std::size_t reference_index = 0;
    std::size_t moving_index = 0;
    int dx = 0;
    int dy = 0;
    double weight = 1.0;
};

struct TilePair {
    std::size_t reference_index = 0;
    std::size_t moving_index = 0;
};

struct TranslationSearchRange {
    int min_dx = 0;
    int max_dx = 0;
    int min_dy = 0;
    int max_dy = 0;
    bool valid = false;
};

struct ColorCorrection {
    double blue = 0.0;
    double green = 0.0;
    double red = 0.0;
};

struct PixelColor {
    int blue = 0;
    int green = 0;
    int red = 0;
};

struct PixelCandidate {
    bool valid = false;
    PixelColor color;
    double score = 0.0;
};

constexpr int kMinimumColorCorrectionSamples = 64;
constexpr int kMaximumColorCorrectionSamplesPerAxis = 128;
constexpr double kMaximumColorCorrection = 48.0;
constexpr int kMaximumConfidenceRadius = 96;
constexpr double kInteriorConfidenceWeight = 0.82;
constexpr double kSharpnessConfidenceWeight = 0.18;
constexpr double kSharpnessScoreScale = 96.0;
constexpr double kBlendScoreTolerance = 0.025;
constexpr double kBlendColorDistanceThreshold = 10.0;
constexpr double kMinimumConstraintConfidence = 0.01;
constexpr int kMaxWideRegistrationEdge = 512;
constexpr int kWideRegistrationCandidates = 12;
constexpr int kWideRegistrationRefinementCandidates = 6;
constexpr int kWideRegistrationTargetSteps = 64;
constexpr double kMicroscopeCrossAxisToleranceFraction = 0.30;
constexpr double kMicroscopeMinimumStepFraction = 0.05;
constexpr double kMicroscopeMaximumStepFraction = 0.98;
constexpr double kMinimumLinearBlendWeight = 0.01;
constexpr double kAutoFeatureConfidenceThreshold = 0.30;
constexpr double kMeasuredOriginalOffsetWeight = 0.05;
constexpr double kEstimatedOriginalOffsetWeight = 0.001;

bool HasReadablePixels(const ImageFrame& frame)
{
    return frame.IsValid() &&
        frame.stride >= frame.width * 3 &&
        frame.bgr.size() >= static_cast<std::size_t>(frame.stride) * static_cast<std::size_t>(frame.height);
}

bool IsUsableConstraintTranslation(const TranslationOffset& translation)
{
    return translation.valid && translation.confidence >= kMinimumConstraintConfidence;
}

bool IsBetterConstraintTranslation(const TranslationOffset& candidate, const TranslationOffset& current)
{
    if (!IsUsableConstraintTranslation(candidate)) {
        return false;
    }
    if (!IsUsableConstraintTranslation(current)) {
        return true;
    }
    if (std::abs(candidate.score - current.score) <= 1e-9) {
        return candidate.confidence > current.confidence;
    }
    return candidate.score < current.score;
}

bool ShouldUseWideRegistrationSearch(
    const ImageFrame& reference,
    const ImageFrame& moving,
    const StitchOptimizationOptions& options)
{
    const int max_edge = std::max({
        reference.width,
        reference.height,
        moving.width,
        moving.height});
    return max_edge <= kMaxWideRegistrationEdge &&
        (options.search_radius_x >= 16 || options.search_radius_y >= 16);
}

int RoundedFraction(int value, double fraction)
{
    return static_cast<int>(std::lround(static_cast<double>(std::max(1, value)) * fraction));
}

enum class SearchDirection {
    Right,
    Left,
    Down,
    Up
};

StitchRegistrationMethod NormalizedRegistrationMethod(const StitchOptimizationOptions& options)
{
    if (!options.use_orb_registration) {
        return StitchRegistrationMethod::Phase;
    }
    return options.registration_method;
}

unsigned char LuminanceByteAt(const ImageFrame& frame, int x, int y)
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

ImageFrame BuildMicroscopyPreprocessedFrame(const ImageFrame& source)
{
    if (!HasReadablePixels(source)) {
        return {};
    }

    const int pixel_count = source.width * source.height;
    std::vector<unsigned char> gray(static_cast<std::size_t>(pixel_count));
    std::array<int, 256> histogram{};
    for (int y = 0; y < source.height; ++y) {
        for (int x = 0; x < source.width; ++x) {
            const unsigned char value = LuminanceByteAt(source, x, y);
            gray[static_cast<std::size_t>(y) * static_cast<std::size_t>(source.width) + static_cast<std::size_t>(x)] = value;
            ++histogram[static_cast<std::size_t>(value)];
        }
    }

    std::array<unsigned char, 256> equalization_map{};
    int cumulative = 0;
    int first_non_zero = 0;
    while (first_non_zero < 255 && histogram[static_cast<std::size_t>(first_non_zero)] == 0) {
        ++first_non_zero;
    }
    const int cdf_min = histogram[static_cast<std::size_t>(first_non_zero)];
    const int denominator = std::max(1, pixel_count - cdf_min);
    for (int value = 0; value < 256; ++value) {
        cumulative += histogram[static_cast<std::size_t>(value)];
        const int mapped = ((cumulative - cdf_min) * 255) / denominator;
        equalization_map[static_cast<std::size_t>(value)] = static_cast<unsigned char>(std::clamp(mapped, 0, 255));
    }

    std::vector<unsigned char> equalized(static_cast<std::size_t>(pixel_count));
    double sum = 0.0;
    double squared_sum = 0.0;
    for (int index = 0; index < pixel_count; ++index) {
        const unsigned char value = equalization_map[static_cast<std::size_t>(gray[static_cast<std::size_t>(index)])];
        equalized[static_cast<std::size_t>(index)] = value;
        sum += value;
        squared_sum += static_cast<double>(value) * static_cast<double>(value);
    }

    const double mean = sum / static_cast<double>(std::max(1, pixel_count));
    const double variance = std::max(0.0, squared_sum / static_cast<double>(std::max(1, pixel_count)) - mean * mean);
    const double stddev = std::sqrt(variance);

    ImageFrame output;
    output.width = source.width;
    output.height = source.height;
    output.stride = (output.width * 3 + 3) & ~3;
    output.timestamp = source.timestamp;
    output.sequence = source.sequence;
    output.bgr.assign(static_cast<std::size_t>(output.stride) * static_cast<std::size_t>(output.height), 0);
    for (int y = 0; y < output.height; ++y) {
        unsigned char* row = output.bgr.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(output.stride);
        for (int x = 0; x < output.width; ++x) {
            const unsigned char equalized_value =
                equalized[static_cast<std::size_t>(y) * static_cast<std::size_t>(output.width) + static_cast<std::size_t>(x)];
            const double normalized = stddev > 1e-6 ?
                128.0 + (static_cast<double>(equalized_value) - mean) * 48.0 / stddev :
                static_cast<double>(equalized_value);
            const unsigned char out = static_cast<unsigned char>(std::lround(std::clamp(normalized, 0.0, 255.0)));
            row[x * 3 + 0] = out;
            row[x * 3 + 1] = out;
            row[x * 3 + 2] = out;
        }
    }
    return output;
}

TranslationSearchRange DirectionalMicroscopeSearchRange(
    const ImageFrame& reference,
    const ImageFrame& moving,
    SearchDirection direction,
    const StitchOptimizationOptions& options)
{
    const int min_width = std::min(reference.width, moving.width);
    const int min_height = std::min(reference.height, moving.height);
    const int min_step_x = std::max(1, RoundedFraction(min_width, kMicroscopeMinimumStepFraction));
    const int min_step_y = std::max(1, RoundedFraction(min_height, kMicroscopeMinimumStepFraction));
    const int max_step_x = std::max(1, RoundedFraction(
        direction == SearchDirection::Left ? moving.width : reference.width,
        kMicroscopeMaximumStepFraction));
    const int max_step_y = std::max(1, RoundedFraction(
        direction == SearchDirection::Up ? moving.height : reference.height,
        kMicroscopeMaximumStepFraction));
    const int drift_x = std::max(options.search_radius_x, RoundedFraction(min_width, kMicroscopeCrossAxisToleranceFraction));
    const int drift_y = std::max(options.search_radius_y, RoundedFraction(min_height, kMicroscopeCrossAxisToleranceFraction));

    TranslationSearchRange range;
    switch (direction) {
    case SearchDirection::Right:
        range.min_dx = min_step_x;
        range.max_dx = max_step_x;
        range.min_dy = -drift_y;
        range.max_dy = drift_y;
        break;
    case SearchDirection::Left:
        range.min_dx = -max_step_x;
        range.max_dx = -min_step_x;
        range.min_dy = -drift_y;
        range.max_dy = drift_y;
        break;
    case SearchDirection::Down:
        range.min_dx = -drift_x;
        range.max_dx = drift_x;
        range.min_dy = min_step_y;
        range.max_dy = max_step_y;
        break;
    case SearchDirection::Up:
        range.min_dx = -drift_x;
        range.max_dx = drift_x;
        range.min_dy = -max_step_y;
        range.max_dy = -min_step_y;
        break;
    }
    range.valid = range.min_dx <= range.max_dx && range.min_dy <= range.max_dy;
    return range;
}

TranslationSearchRange HorizontalMicroscopeSearchRange(
    const ImageFrame& reference,
    const ImageFrame& moving,
    int initial_dx,
    int,
    const StitchOptimizationOptions& options)
{
    return DirectionalMicroscopeSearchRange(
        reference,
        moving,
        initial_dx >= 0 ? SearchDirection::Right : SearchDirection::Left,
        options);
}

TranslationSearchRange VerticalMicroscopeSearchRange(
    const ImageFrame& reference,
    const ImageFrame& moving,
    int,
    int initial_dy,
    const StitchOptimizationOptions& options)
{
    return DirectionalMicroscopeSearchRange(
        reference,
        moving,
        initial_dy >= 0 ? SearchDirection::Down : SearchDirection::Up,
        options);
}

TranslationSearchRange MicroscopeSearchRangeForInitialOffset(
    const ImageFrame& reference,
    const ImageFrame& moving,
    int initial_dx,
    int initial_dy,
    const StitchOptimizationOptions& options)
{
    if (std::abs(initial_dx) >= std::abs(initial_dy) && initial_dx != 0) {
        return HorizontalMicroscopeSearchRange(reference, moving, initial_dx, initial_dy, options);
    }
    if (initial_dy != 0) {
        return VerticalMicroscopeSearchRange(reference, moving, initial_dx, initial_dy, options);
    }

    TranslationSearchRange range;
    range.min_dx = -options.search_radius_x;
    range.max_dx = options.search_radius_x;
    range.min_dy = -options.search_radius_y;
    range.max_dy = options.search_radius_y;
    range.valid = true;
    return range;
}

bool SameSearchRange(const TranslationSearchRange& left, const TranslationSearchRange& right)
{
    return left.valid == right.valid &&
        left.min_dx == right.min_dx &&
        left.max_dx == right.max_dx &&
        left.min_dy == right.min_dy &&
        left.max_dy == right.max_dy;
}

void AppendUniqueSearchRange(std::vector<TranslationSearchRange>& ranges, TranslationSearchRange range)
{
    if (!range.valid) {
        return;
    }
    const bool exists = std::any_of(
        ranges.begin(),
        ranges.end(),
        [&](const TranslationSearchRange& existing) { return SameSearchRange(existing, range); });
    if (!exists) {
        ranges.push_back(range);
    }
}

int OffsetStepForRange(const TranslationSearchRange& range)
{
    return std::clamp(
        std::max(range.max_dx - range.min_dx, range.max_dy - range.min_dy) / kWideRegistrationTargetSteps,
        2,
        8);
}

TranslationOffset BetterTranslation(const TranslationOffset& left, const TranslationOffset& right)
{
    return IsBetterConstraintTranslation(left, right) ? left : right;
}

TranslationOffset RefineCandidate(
    const ImageFrame& reference,
    const ImageFrame& moving,
    const TranslationOffset& candidate,
    int radius_x,
    int radius_y)
{
    if (!IsUsableConstraintTranslation(candidate)) {
        return {};
    }
    const TranslationOffset refined = ImageRegistration::RefineTranslation(
        reference,
        moving,
        candidate.dx,
        candidate.dy,
        radius_x,
        radius_y);
    if (IsUsableConstraintTranslation(refined)) {
        return BetterTranslation(refined, candidate);
    }
    return candidate;
}

TranslationOffset EstimatePhaseTranslationInRange(
    const ImageFrame& reference,
    const ImageFrame& moving,
    const TranslationSearchRange& range)
{
    if (!range.valid) {
        return {};
    }

    const int offset_step = OffsetStepForRange(range);
    const std::vector<TranslationOffset> candidates = ImageRegistration::EstimateTranslationCandidates(
        reference,
        moving,
        range.min_dx,
        range.max_dx,
        range.min_dy,
        range.max_dy,
        kWideRegistrationCandidates,
        offset_step,
        offset_step);
    TranslationOffset best;
    const int refinement_count = std::min(
        static_cast<int>(candidates.size()),
        kWideRegistrationRefinementCandidates);
    for (int index = 0; index < refinement_count; ++index) {
        const TranslationOffset refined = RefineCandidate(
            reference,
            moving,
            candidates[static_cast<std::size_t>(index)],
            std::max(2, offset_step * 2),
            std::max(2, offset_step * 2));
        if (IsBetterConstraintTranslation(refined, best)) {
            best = refined;
        }
    }
    return best;
}

TranslationOffset EstimateFeatureTranslationInRange(
    const ImageFrame& reference,
    const ImageFrame& moving,
    const TranslationSearchRange& range)
{
    if (!range.valid) {
        return {};
    }
    const int offset_step = OffsetStepForRange(range);
    const TranslationOffset candidate = ImageRegistration::EstimateOrbTranslation(
        reference,
        moving,
        range.min_dx,
        range.max_dx,
        range.min_dy,
        range.max_dy);
    return RefineCandidate(
        reference,
        moving,
        candidate,
        std::max(2, std::min(12, offset_step * 3)),
        std::max(2, std::min(12, offset_step * 3)));
}

TranslationOffset EstimateMicroscopyTranslationInRange(
    const ImageFrame& reference,
    const ImageFrame& moving,
    const TranslationSearchRange& range)
{
    TranslationOffset best = EstimateFeatureTranslationInRange(reference, moving, range);
    if (IsUsableConstraintTranslation(best) && best.confidence >= kAutoFeatureConfidenceThreshold) {
        return best;
    }

    const ImageFrame reference_preprocessed = BuildMicroscopyPreprocessedFrame(reference);
    const ImageFrame moving_preprocessed = BuildMicroscopyPreprocessedFrame(moving);
    const TranslationOffset preprocessed_phase = EstimatePhaseTranslationInRange(
        reference_preprocessed.IsValid() ? reference_preprocessed : reference,
        moving_preprocessed.IsValid() ? moving_preprocessed : moving,
        range);
    if (IsUsableConstraintTranslation(preprocessed_phase)) {
        const int offset_step = OffsetStepForRange(range);
        TranslationOffset refined = ImageRegistration::RefineTranslation(
            reference,
            moving,
            preprocessed_phase.dx,
            preprocessed_phase.dy,
            std::max(3, offset_step * 2),
            std::max(3, offset_step * 2));
        if (!IsUsableConstraintTranslation(refined)) {
            refined = preprocessed_phase;
        } else {
            refined.confidence = std::max(refined.confidence, preprocessed_phase.confidence);
        }
        if (IsBetterConstraintTranslation(refined, best)) {
            best = refined;
        }
    }

    if (!IsUsableConstraintTranslation(best)) {
        best = EstimatePhaseTranslationInRange(reference, moving, range);
    }
    return best;
}

TranslationOffset EstimateMethodTranslationInRange(
    const ImageFrame& reference,
    const ImageFrame& moving,
    const TranslationSearchRange& range,
    StitchRegistrationMethod method)
{
    switch (method) {
    case StitchRegistrationMethod::Feature:
        return EstimateFeatureTranslationInRange(reference, moving, range);
    case StitchRegistrationMethod::Auto: {
        const TranslationOffset feature = EstimateFeatureTranslationInRange(reference, moving, range);
        if (IsUsableConstraintTranslation(feature) && feature.confidence >= kAutoFeatureConfidenceThreshold) {
            return feature;
        }
        const TranslationOffset phase = EstimatePhaseTranslationInRange(reference, moving, range);
        return IsUsableConstraintTranslation(phase) ? phase : feature;
    }
    case StitchRegistrationMethod::Sift:
    case StitchRegistrationMethod::Micro:
        return EstimateMicroscopyTranslationInRange(reference, moving, range);
    case StitchRegistrationMethod::Phase:
    default:
        return EstimatePhaseTranslationInRange(reference, moving, range);
    }
}

std::vector<TranslationSearchRange> SearchRangesForPair(
    const ImageFrame& reference,
    const ImageFrame& moving,
    int initial_dx,
    int initial_dy,
    StitchRegistrationMethod method,
    const StitchOptimizationOptions& options)
{
    std::vector<TranslationSearchRange> ranges;
    AppendUniqueSearchRange(
        ranges,
        MicroscopeSearchRangeForInitialOffset(reference, moving, initial_dx, initial_dy, options));

    if (method == StitchRegistrationMethod::Auto ||
        method == StitchRegistrationMethod::Sift ||
        method == StitchRegistrationMethod::Micro) {
        for (SearchDirection direction :
             {SearchDirection::Right, SearchDirection::Left, SearchDirection::Down, SearchDirection::Up}) {
            AppendUniqueSearchRange(
                ranges,
                DirectionalMicroscopeSearchRange(reference, moving, direction, options));
        }
    }
    return ranges;
}

TranslationOffset EstimateConstraintTranslation(
    const ImageFrame& reference,
    const ImageFrame& moving,
    int initial_dx,
    int initial_dy,
    const StitchOptimizationOptions& options)
{
    const StitchRegistrationMethod method = NormalizedRegistrationMethod(options);
    TranslationOffset best;
    if (method != StitchRegistrationMethod::Feature) {
        best = ImageRegistration::RefineTranslation(
            reference,
            moving,
            initial_dx,
            initial_dy,
            options.search_radius_x,
            options.search_radius_y);
    }

    if (!ShouldUseWideRegistrationSearch(reference, moving, options)) {
        TranslationSearchRange local_range;
        local_range.min_dx = initial_dx - options.search_radius_x;
        local_range.max_dx = initial_dx + options.search_radius_x;
        local_range.min_dy = initial_dy - options.search_radius_y;
        local_range.max_dy = initial_dy + options.search_radius_y;
        local_range.valid = true;
        const TranslationOffset candidate = EstimateMethodTranslationInRange(reference, moving, local_range, method);
        if (IsBetterConstraintTranslation(candidate, best)) {
            best = candidate;
        }
        return best;
    }

    const std::vector<TranslationSearchRange> ranges = SearchRangesForPair(
        reference,
        moving,
        initial_dx,
        initial_dy,
        method,
        options);
    for (const TranslationSearchRange& range : ranges) {
        const TranslationOffset candidate = EstimateMethodTranslationInRange(reference, moving, range, method);
        if (IsBetterConstraintTranslation(candidate, best)) {
            best = candidate;
        }
    }
    return best;
}
bool ExpandBounds(const StitchTile& tile, Bounds& bounds, bool first)
{
    if (!HasReadablePixels(tile.frame)) {
        return false;
    }

    const int left = tile.offset_x;
    const int top = tile.offset_y;
    const int right = tile.offset_x + tile.frame.width;
    const int bottom = tile.offset_y + tile.frame.height;
    if (first) {
        bounds = Bounds{left, top, right, bottom};
    } else {
        bounds.left = std::min(bounds.left, left);
        bounds.top = std::min(bounds.top, top);
        bounds.right = std::max(bounds.right, right);
        bounds.bottom = std::max(bounds.bottom, bottom);
    }
    return true;
}

PixelColor PixelAt(const ImageFrame& frame, int x, int y)
{
    const unsigned char* pixel =
        frame.bgr.data() +
        static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.stride) +
        static_cast<std::size_t>(x) * 3U;
    return PixelColor{pixel[0], pixel[1], pixel[2]};
}

PixelColor PixelAtClamped(const ImageFrame& frame, int x, int y)
{
    return PixelAt(
        frame,
        std::clamp(x, 0, frame.width - 1),
        std::clamp(y, 0, frame.height - 1));
}

double ClampChannel(double value)
{
    return std::clamp(value, 0.0, 255.0);
}

PixelColor CorrectedPixelAt(const StitchTile& tile, const ColorCorrection& correction, int x, int y)
{
    const PixelColor pixel = PixelAt(tile.frame, x, y);
    return PixelColor{
        static_cast<int>(std::lround(ClampChannel(pixel.blue + correction.blue))),
        static_cast<int>(std::lround(ClampChannel(pixel.green + correction.green))),
        static_cast<int>(std::lround(ClampChannel(pixel.red + correction.red)))};
}

double Luminance(const PixelColor& pixel)
{
    return (static_cast<double>(pixel.red) * 77.0 +
            static_cast<double>(pixel.green) * 150.0 +
            static_cast<double>(pixel.blue) * 29.0) /
        256.0;
}

double LocalSharpnessAt(const ImageFrame& frame, int x, int y)
{
    const double left = Luminance(PixelAtClamped(frame, x - 1, y));
    const double right = Luminance(PixelAtClamped(frame, x + 1, y));
    const double top = Luminance(PixelAtClamped(frame, x, y - 1));
    const double bottom = Luminance(PixelAtClamped(frame, x, y + 1));
    return std::abs(right - left) + std::abs(bottom - top);
}

double SmoothStep(double value)
{
    const double normalized = std::clamp(value, 0.0, 1.0);
    return normalized * normalized * (3.0 - 2.0 * normalized);
}

int EdgeDistance(const StitchTile& tile, int x, int y)
{
    return std::min({
        x + 1,
        y + 1,
        tile.frame.width - x,
        tile.frame.height - y});
}

double LinearBlendWeight(const StitchTile& tile, int x, int y)
{
    if (x < 0 || y < 0 || x >= tile.frame.width || y >= tile.frame.height) {
        return 0.0;
    }
    const int edge_distance = std::max(0, EdgeDistance(tile, x, y) - 1);
    const int max_distance = std::max(1, std::min(tile.frame.width, tile.frame.height) / 2);
    return std::max(
        kMinimumLinearBlendWeight,
        std::min(1.0, static_cast<double>(edge_distance) / static_cast<double>(max_distance)));
}

double InteriorConfidence(const StitchTile& tile, int x, int y)
{
    const int confidence_radius = std::max(
        1,
        std::min(kMaximumConfidenceRadius, std::min(tile.frame.width, tile.frame.height) / 5));
    return std::max(
        0.05,
        SmoothStep(static_cast<double>(EdgeDistance(tile, x, y)) / static_cast<double>(confidence_radius)));
}

PixelCandidate CandidateForPixel(
    const StitchTile& tile,
    const ColorCorrection& correction,
    int x,
    int y)
{
    if (x < 0 || y < 0 || x >= tile.frame.width || y >= tile.frame.height) {
        return {};
    }

    const double interior = InteriorConfidence(tile, x, y);
    const double sharpness = std::clamp(
        LocalSharpnessAt(tile.frame, x, y) / kSharpnessScoreScale,
        0.0,
        1.0);
    PixelCandidate candidate;
    candidate.valid = true;
    candidate.color = CorrectedPixelAt(tile, correction, x, y);
    candidate.score =
        interior * kInteriorConfidenceWeight +
        sharpness * kSharpnessConfidenceWeight +
        static_cast<double>(EdgeDistance(tile, x, y)) * 0.000001;
    return candidate;
}

double ColorDistance(const PixelColor& left, const PixelColor& right)
{
    return (
        std::abs(left.blue - right.blue) +
        std::abs(left.green - right.green) +
        std::abs(left.red - right.red)) / 3.0;
}

PixelColor BlendCandidates(const PixelCandidate& primary, const PixelCandidate& secondary)
{
    const double primary_weight = std::max(0.01, primary.score);
    const double secondary_weight = std::max(0.01, secondary.score);
    const double total_weight = primary_weight + secondary_weight;
    return PixelColor{
        static_cast<int>(std::lround(ClampChannel(
            (primary.color.blue * primary_weight + secondary.color.blue * secondary_weight) / total_weight))),
        static_cast<int>(std::lround(ClampChannel(
            (primary.color.green * primary_weight + secondary.color.green * secondary_weight) / total_weight))),
        static_cast<int>(std::lround(ClampChannel(
            (primary.color.red * primary_weight + secondary.color.red * secondary_weight) / total_weight)))};
}

bool ShouldBlendCandidates(const PixelCandidate& primary, const PixelCandidate& secondary)
{
    return primary.valid &&
        secondary.valid &&
        std::abs(primary.score - secondary.score) <= kBlendScoreTolerance &&
        ColorDistance(primary.color, secondary.color) <= kBlendColorDistanceThreshold;
}

bool TileOverlap(
    const StitchTile& left_tile,
    const StitchTile& right_tile,
    int& left,
    int& top,
    int& right,
    int& bottom)
{
    left = std::max(left_tile.offset_x, right_tile.offset_x);
    top = std::max(left_tile.offset_y, right_tile.offset_y);
    right = std::min(left_tile.offset_x + left_tile.frame.width, right_tile.offset_x + right_tile.frame.width);
    bottom = std::min(left_tile.offset_y + left_tile.frame.height, right_tile.offset_y + right_tile.frame.height);
    return right > left && bottom > top;
}

ColorCorrection EstimateTileColorCorrection(
    const StitchTile& tile,
    const std::vector<StitchTile>& tiles,
    const std::vector<ColorCorrection>& corrections,
    std::size_t tile_index)
{
    double blue_delta = 0.0;
    double green_delta = 0.0;
    double red_delta = 0.0;
    int samples = 0;

    for (std::size_t reference_index = 0; reference_index < tile_index; ++reference_index) {
        const StitchTile& reference = tiles[reference_index];
        if (!HasReadablePixels(reference.frame)) {
            continue;
        }

        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;
        if (!TileOverlap(reference, tile, left, top, right, bottom)) {
            continue;
        }

        const int overlap_width = right - left;
        const int overlap_height = bottom - top;
        const int step_x = std::max(1, overlap_width / kMaximumColorCorrectionSamplesPerAxis);
        const int step_y = std::max(1, overlap_height / kMaximumColorCorrectionSamplesPerAxis);
        for (int canvas_y = top; canvas_y < bottom; canvas_y += step_y) {
            for (int canvas_x = left; canvas_x < right; canvas_x += step_x) {
                const PixelColor reference_pixel = CorrectedPixelAt(
                    reference,
                    corrections[reference_index],
                    canvas_x - reference.offset_x,
                    canvas_y - reference.offset_y);
                const PixelColor moving_pixel = PixelAt(
                    tile.frame,
                    canvas_x - tile.offset_x,
                    canvas_y - tile.offset_y);
                blue_delta += reference_pixel.blue - moving_pixel.blue;
                green_delta += reference_pixel.green - moving_pixel.green;
                red_delta += reference_pixel.red - moving_pixel.red;
                ++samples;
            }
        }
    }

    if (samples < kMinimumColorCorrectionSamples) {
        return {};
    }

    return ColorCorrection{
        std::clamp(blue_delta / samples, -kMaximumColorCorrection, kMaximumColorCorrection),
        std::clamp(green_delta / samples, -kMaximumColorCorrection, kMaximumColorCorrection),
        std::clamp(red_delta / samples, -kMaximumColorCorrection, kMaximumColorCorrection)};
}

std::vector<ColorCorrection> EstimateColorCorrections(const std::vector<StitchTile>& tiles)
{
    std::vector<ColorCorrection> corrections(tiles.size());
    for (std::size_t index = 1; index < tiles.size(); ++index) {
        if (HasReadablePixels(tiles[index].frame)) {
            corrections[index] = EstimateTileColorCorrection(tiles[index], tiles, corrections, index);
        }
    }
    return corrections;
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
bool SameTilePair(const TilePair& left, const TilePair& right)
{
    return left.reference_index == right.reference_index && left.moving_index == right.moving_index;
}

void AppendUniqueTilePair(std::vector<TilePair>& pairs, TilePair pair)
{
    if (pair.reference_index == pair.moving_index) {
        return;
    }
    const bool exists = std::any_of(
        pairs.begin(),
        pairs.end(),
        [&](const TilePair& existing) { return SameTilePair(existing, pair); });
    if (!exists) {
        pairs.push_back(pair);
    }
}

bool IsLikelyMicroscopeNeighbor(const StitchTile& reference, const StitchTile& moving)
{
    if (!HasReadablePixels(reference.frame) || !HasReadablePixels(moving.frame)) {
        return false;
    }

    const int dx = moving.offset_x - reference.offset_x;
    const int dy = moving.offset_y - reference.offset_y;
    const int min_width = std::min(reference.frame.width, moving.frame.width);
    const int min_height = std::min(reference.frame.height, moving.frame.height);
    const int min_step_x = std::max(1, RoundedFraction(min_width, kMicroscopeMinimumStepFraction));
    const int min_step_y = std::max(1, RoundedFraction(min_height, kMicroscopeMinimumStepFraction));
    const int max_step_x = std::max(1, RoundedFraction(std::max(reference.frame.width, moving.frame.width), kMicroscopeMaximumStepFraction));
    const int max_step_y = std::max(1, RoundedFraction(std::max(reference.frame.height, moving.frame.height), kMicroscopeMaximumStepFraction));
    const int drift_x = std::max(1, RoundedFraction(min_width, kMicroscopeCrossAxisToleranceFraction));
    const int drift_y = std::max(1, RoundedFraction(min_height, kMicroscopeCrossAxisToleranceFraction));

    const bool horizontal = std::abs(dx) >= min_step_x && std::abs(dx) <= max_step_x && std::abs(dy) <= drift_y;
    const bool vertical = std::abs(dy) >= min_step_y && std::abs(dy) <= max_step_y && std::abs(dx) <= drift_x;
    return horizontal || vertical;
}

std::vector<TilePair> BuildMicroscopeNeighborPairs(const std::vector<StitchTile>& tiles)
{
    std::vector<TilePair> pairs;
    if (tiles.size() < 2) {
        return pairs;
    }

    pairs.reserve(tiles.size() * 2U);
    for (std::size_t index = 1; index < tiles.size(); ++index) {
        AppendUniqueTilePair(pairs, TilePair{index - 1U, index});
    }

    for (std::size_t reference_index = 0; reference_index < tiles.size(); ++reference_index) {
        for (std::size_t moving_index = reference_index + 1U; moving_index < tiles.size(); ++moving_index) {
            if (IsLikelyMicroscopeNeighbor(tiles[reference_index], tiles[moving_index])) {
                AppendUniqueTilePair(pairs, TilePair{reference_index, moving_index});
            }
        }
    }
    return pairs;
}

} // namespace

StitchOptimizationResult ImageStitcher::OptimizeTileOffsets(
    const std::vector<StitchTile>& tiles,
    StitchOptimizationOptions options,
    const std::atomic_bool* cancel_requested,
    const std::function<void(int)>& progress_callback)
{
    StitchOptimizationResult result;
    result.tiles = tiles;
    ReportProgress(progress_callback, 0);
    if (tiles.size() < 2 || IsCancelled(cancel_requested)) {
        return result;
    }

    options.search_radius_x = std::max(0, options.search_radius_x);
    options.search_radius_y = std::max(0, options.search_radius_y);
    options.iterations = std::max(1, options.iterations);

    const std::vector<TilePair> tile_pairs = BuildMicroscopeNeighborPairs(tiles);
    const int pair_count = static_cast<int>(tile_pairs.size());
    int processed_pairs = 0;
    std::vector<TileConstraint> constraints;
    constraints.reserve(static_cast<std::size_t>(pair_count));

    for (const TilePair& pair : tile_pairs) {
        if (IsCancelled(cancel_requested)) {
            return result;
        }
        ++processed_pairs;
        const std::size_t reference_index = pair.reference_index;
        const std::size_t moving_index = pair.moving_index;
        if (reference_index >= tiles.size() ||
            moving_index >= tiles.size() ||
            !tiles[reference_index].frame.IsValid() ||
            !tiles[moving_index].frame.IsValid()) {
            ReportProgress(progress_callback, (processed_pairs * 70) / std::max(1, pair_count));
            continue;
        }

        const int initial_dx = tiles[moving_index].offset_x - tiles[reference_index].offset_x;
        const int initial_dy = tiles[moving_index].offset_y - tiles[reference_index].offset_y;
        const TranslationOffset refined = EstimateConstraintTranslation(
            tiles[reference_index].frame,
            tiles[moving_index].frame,
            initial_dx,
            initial_dy,
            options);
        if (IsUsableConstraintTranslation(refined)) {
            TileConstraint constraint;
            constraint.reference_index = reference_index;
            constraint.moving_index = moving_index;
            constraint.dx = refined.dx;
            constraint.dy = refined.dy;
            constraint.weight =
                std::max(0.05, refined.confidence) / (1.0 + std::max(0.0, refined.score));
            constraints.push_back(constraint);
        }
        ReportProgress(progress_callback, (processed_pairs * 70) / std::max(1, pair_count));
    }
    result.constraint_count = static_cast<int>(constraints.size());
    if (constraints.empty() || IsCancelled(cancel_requested)) {
        ReportProgress(progress_callback, 100);
        return result;
    }

    std::vector<double> x(tiles.size(), 0.0);
    std::vector<double> y(tiles.size(), 0.0);
    std::vector<double> original_x(tiles.size(), 0.0);
    std::vector<double> original_y(tiles.size(), 0.0);
    for (std::size_t index = 0; index < tiles.size(); ++index) {
        x[index] = original_x[index] = static_cast<double>(tiles[index].offset_x);
        y[index] = original_y[index] = static_cast<double>(tiles[index].offset_y);
    }
    for (int iteration = 0; iteration < options.iterations; ++iteration) {
        if (IsCancelled(cancel_requested)) {
            return result;
        }

        std::vector<double> next_x = x;
        std::vector<double> next_y = y;
        for (std::size_t tile_index = 1; tile_index < tiles.size(); ++tile_index) {
            const double original_offset_weight = tiles[tile_index].estimated_position ?
                kEstimatedOriginalOffsetWeight :
                kMeasuredOriginalOffsetWeight;
            double sum_x = original_x[tile_index] * original_offset_weight;
            double sum_y = original_y[tile_index] * original_offset_weight;
            double sum_weight = original_offset_weight;

            for (const TileConstraint& constraint : constraints) {
                if (constraint.reference_index == tile_index) {
                    sum_x += (x[constraint.moving_index] - constraint.dx) * constraint.weight;
                    sum_y += (y[constraint.moving_index] - constraint.dy) * constraint.weight;
                    sum_weight += constraint.weight;
                } else if (constraint.moving_index == tile_index) {
                    sum_x += (x[constraint.reference_index] + constraint.dx) * constraint.weight;
                    sum_y += (y[constraint.reference_index] + constraint.dy) * constraint.weight;
                    sum_weight += constraint.weight;
                }
            }

            if (sum_weight > 0.0) {
                next_x[tile_index] = sum_x / sum_weight;
                next_y[tile_index] = sum_y / sum_weight;
            }
        }

        x = std::move(next_x);
        y = std::move(next_y);
        x[0] = original_x[0];
        y[0] = original_y[0];
        ReportProgress(
            progress_callback,
            70 + ((iteration + 1) * 30) / options.iterations);
    }

    for (std::size_t index = 0; index < result.tiles.size(); ++index) {
        result.tiles[index].offset_x = static_cast<int>(std::lround(x[index]));
        result.tiles[index].offset_y = static_cast<int>(std::lround(y[index]));
    }
    result.optimized = true;
    ReportProgress(progress_callback, 100);
    return result;
}

ImageFrame ImageStitcher::StitchAverage(
    const std::vector<StitchTile>& tiles,
    StitchBlendMode blend_mode,
    const std::atomic_bool* cancel_requested,
    const std::function<void(int)>& progress_callback)
{
    ReportProgress(progress_callback, 0);
    if (IsCancelled(cancel_requested)) {
        return {};
    }

    Bounds bounds;
    bool has_tile = false;
    for (const StitchTile& tile : tiles) {
        if (IsCancelled(cancel_requested)) {
            return {};
        }
        if (ExpandBounds(tile, bounds, !has_tile)) {
            has_tile = true;
        }
    }
    if (!has_tile || bounds.right <= bounds.left || bounds.bottom <= bounds.top) {
        return {};
    }
    ReportProgress(progress_callback, 5);

    ImageFrame output;
    output.width = bounds.right - bounds.left;
    output.height = bounds.bottom - bounds.top;
    output.stride = (output.width * 3 + 3) & ~3;
    output.bgr.assign(static_cast<std::size_t>(output.stride) * static_cast<std::size_t>(output.height), 0);

    const std::vector<ColorCorrection> corrections = EstimateColorCorrections(tiles);
    std::vector<std::size_t> readable_tile_indices;
    readable_tile_indices.reserve(tiles.size());
    for (std::size_t index = 0; index < tiles.size(); ++index) {
        if (HasReadablePixels(tiles[index].frame)) {
            readable_tile_indices.push_back(index);
        }
    }
    std::vector<std::size_t> active_tile_indices;
    active_tile_indices.reserve(readable_tile_indices.size());
    for (int y = 0; y < output.height; ++y) {
        if (IsCancelled(cancel_requested)) {
            return {};
        }
        unsigned char* dst = output.bgr.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(output.stride);
        const int canvas_y = bounds.top + y;
        active_tile_indices.clear();
        for (std::size_t tile_index : readable_tile_indices) {
            const StitchTile& tile = tiles[tile_index];
            if (canvas_y >= tile.offset_y && canvas_y < tile.offset_y + tile.frame.height) {
                active_tile_indices.push_back(tile_index);
            }
        }
        for (int x = 0; x < output.width; ++x) {
            const int canvas_x = bounds.left + x;
            double blue_sum = 0.0;
            double green_sum = 0.0;
            double red_sum = 0.0;
            double weight_sum = 0.0;

            for (std::size_t tile_index : active_tile_indices) {
                const StitchTile& tile = tiles[tile_index];
                const int tile_x = canvas_x - tile.offset_x;
                const int tile_y = canvas_y - tile.offset_y;
                if (tile_x < 0 || tile_y < 0 || tile_x >= tile.frame.width || tile_y >= tile.frame.height) {
                    continue;
                }

                const double weight = blend_mode == StitchBlendMode::Linear
                    ? LinearBlendWeight(tile, tile_x, tile_y)
                    : 1.0;
                if (weight <= 0.0) {
                    continue;
                }
                const PixelColor color = CorrectedPixelAt(tile, corrections[tile_index], tile_x, tile_y);
                blue_sum += static_cast<double>(color.blue) * weight;
                green_sum += static_cast<double>(color.green) * weight;
                red_sum += static_cast<double>(color.red) * weight;
                weight_sum += weight;
            }

            if (weight_sum <= 0.0) {
                continue;
            }

            dst[x * 3 + 0] = static_cast<unsigned char>(std::lround(ClampChannel(blue_sum / weight_sum)));
            dst[x * 3 + 1] = static_cast<unsigned char>(std::lround(ClampChannel(green_sum / weight_sum)));
            dst[x * 3 + 2] = static_cast<unsigned char>(std::lround(ClampChannel(red_sum / weight_sum)));
        }
        ReportProgress(progress_callback, 5 + ((y + 1) * 95) / output.height);
    }

    ReportProgress(progress_callback, 100);
    return output;
}
