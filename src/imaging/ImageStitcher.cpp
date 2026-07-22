#include "ImageStitcher.h"

#include "ImageRegistration.h"

#include <algorithm>
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
constexpr int kWideRegistrationCandidates = 8;
constexpr int kWideRegistrationRefinementCandidates = 4;
constexpr int kWideRegistrationTargetSteps = 64;

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

TranslationOffset EstimateConstraintTranslation(
    const ImageFrame& reference,
    const ImageFrame& moving,
    int initial_dx,
    int initial_dy,
    const StitchOptimizationOptions& options)
{
    TranslationOffset best = ImageRegistration::RefineTranslation(
        reference,
        moving,
        initial_dx,
        initial_dy,
        options.search_radius_x,
        options.search_radius_y);

    if (!ShouldUseWideRegistrationSearch(reference, moving, options)) {
        return best;
    }

    const int wide_radius_x = std::max(options.search_radius_x, std::max(reference.width, moving.width));
    const int wide_radius_y = std::max(options.search_radius_y, std::max(reference.height, moving.height));
    const int offset_step = std::clamp(
        std::max(wide_radius_x, wide_radius_y) / kWideRegistrationTargetSteps,
        2,
        8);

    const TranslationOffset orb_candidate = ImageRegistration::EstimateOrbTranslation(
        reference,
        moving,
        -wide_radius_x,
        wide_radius_x,
        -wide_radius_y,
        wide_radius_y);
    if (IsUsableConstraintTranslation(orb_candidate)) {
        const int orb_refinement_radius = std::max(2, std::min(12, offset_step * 3));
        const TranslationOffset orb_refined = ImageRegistration::RefineTranslation(
            reference,
            moving,
            orb_candidate.dx,
            orb_candidate.dy,
            orb_refinement_radius,
            orb_refinement_radius);
        if (IsBetterConstraintTranslation(orb_refined, best)) {
            best = orb_refined;
        }
    }

    const std::vector<TranslationOffset> candidates = ImageRegistration::EstimateTranslationCandidates(
        reference,
        moving,
        -wide_radius_x,
        wide_radius_x,
        -wide_radius_y,
        wide_radius_y,
        kWideRegistrationCandidates,
        offset_step,
        offset_step);
    const int refinement_count = std::min(
        static_cast<int>(candidates.size()),
        kWideRegistrationRefinementCandidates);
    for (int index = 0; index < refinement_count; ++index) {
        const TranslationOffset& candidate = candidates[static_cast<std::size_t>(index)];
        const TranslationOffset refined = ImageRegistration::RefineTranslation(
            reference,
            moving,
            candidate.dx,
            candidate.dy,
            std::max(2, offset_step * 2),
            std::max(2, offset_step * 2));
        if (IsBetterConstraintTranslation(refined, best)) {
            best = refined;
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

    const int pair_count = static_cast<int>((tiles.size() * (tiles.size() - 1U)) / 2U);
    int processed_pairs = 0;
    std::vector<TileConstraint> constraints;
    constraints.reserve(static_cast<std::size_t>(pair_count));

    for (std::size_t reference_index = 0; reference_index < tiles.size(); ++reference_index) {
        if (!tiles[reference_index].frame.IsValid()) {
            continue;
        }
        for (std::size_t moving_index = reference_index + 1U; moving_index < tiles.size(); ++moving_index) {
            if (IsCancelled(cancel_requested)) {
                return result;
            }
            ++processed_pairs;
            if (!tiles[moving_index].frame.IsValid()) {
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

    constexpr double kOriginalOffsetWeight = 0.05;
    for (int iteration = 0; iteration < options.iterations; ++iteration) {
        if (IsCancelled(cancel_requested)) {
            return result;
        }

        std::vector<double> next_x = x;
        std::vector<double> next_y = y;
        for (std::size_t tile_index = 1; tile_index < tiles.size(); ++tile_index) {
            double sum_x = original_x[tile_index] * kOriginalOffsetWeight;
            double sum_y = original_y[tile_index] * kOriginalOffsetWeight;
            double sum_weight = kOriginalOffsetWeight;

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
            PixelCandidate best;
            PixelCandidate second_best;

            for (std::size_t tile_index : active_tile_indices) {
                const StitchTile& tile = tiles[tile_index];
                const PixelCandidate candidate = CandidateForPixel(
                    tile,
                    corrections[tile_index],
                    canvas_x - tile.offset_x,
                    canvas_y - tile.offset_y);
                if (!candidate.valid) {
                    continue;
                }
                if (!best.valid || candidate.score > best.score) {
                    second_best = best;
                    best = candidate;
                } else if (!second_best.valid || candidate.score > second_best.score) {
                    second_best = candidate;
                }
            }

            if (!best.valid) {
                continue;
            }

            PixelColor output_color = best.color;
            if (ShouldBlendCandidates(best, second_best)) {
                output_color = BlendCandidates(best, second_best);
            }
            dst[x * 3 + 0] = static_cast<unsigned char>(output_color.blue);
            dst[x * 3 + 1] = static_cast<unsigned char>(output_color.green);
            dst[x * 3 + 2] = static_cast<unsigned char>(output_color.red);
        }
        ReportProgress(progress_callback, 5 + ((y + 1) * 95) / output.height);
    }

    ReportProgress(progress_callback, 100);
    return output;
}
