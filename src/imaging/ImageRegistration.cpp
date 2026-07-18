#include "ImageRegistration.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <vector>

namespace {

struct OffsetScore {
    bool valid = false;
    double error = std::numeric_limits<double>::infinity();
    int overlap_pixels = 0;
};

constexpr int kTargetSamplesPerOffset = 2048;
constexpr int kMinimumSamplesPerOffset = 8;
constexpr double kMinimumOverlapAreaFraction = 0.15;
constexpr double kMinimumLuminanceEnergyPerSample = 4.0;
constexpr double kMinimumHighPassEnergyPerSample = 1.0;
constexpr double kMinimumGradientEnergyPerSample = 4.0;
constexpr double kLuminanceScoreWeight = 0.20;
constexpr double kHighPassScoreWeight = 0.35;
constexpr double kGradientScoreWeight = 0.45;
constexpr double kCorrelationScoreScale = 64.0;
constexpr double kOverlapPenaltyScale = 0.15;
constexpr int kMaxPhaseCorrelationPixels = 1024 * 1024;
constexpr int kMaxPhaseCorrelationCandidates = 16;
constexpr int kMinimumPhaseCorrelationSearchPositions = 512;
constexpr double kMinimumPhaseInputEnergy = 1.0;
constexpr double kPhaseMagnitudeEpsilon = 1e-9;
constexpr double kPi = 3.14159265358979323846;

using Complex = std::complex<double>;

bool HasReadablePixels(const ImageFrame& frame)
{
    return frame.IsValid() &&
        frame.stride >= frame.width * 3 &&
        frame.bgr.size() >= static_cast<std::size_t>(frame.stride) * static_cast<std::size_t>(frame.height);
}

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

double LuminanceAtClamped(const ImageFrame& frame, int x, int y)
{
    return static_cast<double>(LuminanceAt(
        frame,
        std::clamp(x, 0, frame.width - 1),
        std::clamp(y, 0, frame.height - 1)));
}

void GradientAt(const ImageFrame& frame, int x, int y, double& gx, double& gy)
{
    gx = (LuminanceAtClamped(frame, x + 1, y) - LuminanceAtClamped(frame, x - 1, y)) * 0.5;
    gy = (LuminanceAtClamped(frame, x, y + 1) - LuminanceAtClamped(frame, x, y - 1)) * 0.5;
}

double HighPassAt(const ImageFrame& frame, int x, int y)
{
    const double center = LuminanceAtClamped(frame, x, y);
    const double axial_mean =
        (LuminanceAtClamped(frame, x - 1, y) +
         LuminanceAtClamped(frame, x + 1, y) +
         LuminanceAtClamped(frame, x, y - 1) +
         LuminanceAtClamped(frame, x, y + 1)) * 0.25;
    return center - axial_mean;
}

double WindowWeight(int x, int y, int width, int height);

double StructuralSampleWeight(
    int x,
    int y,
    int width,
    int height,
    double reference_gx,
    double reference_gy,
    double moving_gx,
    double moving_gy)
{
    const double reference_gradient = std::sqrt(reference_gx * reference_gx + reference_gy * reference_gy);
    const double moving_gradient = std::sqrt(moving_gx * moving_gx + moving_gy * moving_gy);
    const double shared_texture = std::min(reference_gradient, moving_gradient);
    const double texture_boost = 0.20 + std::min(4.0, shared_texture / 12.0);
    return WindowWeight(x, y, width, height) * texture_boost;
}

int PixelStepForOverlap(int width, int height)
{
    const int overlap_pixels = width * height;
    if (overlap_pixels <= kTargetSamplesPerOffset) {
        return 1;
    }

    return std::max(
        1,
        static_cast<int>(std::sqrt(static_cast<double>(overlap_pixels) / kTargetSamplesPerOffset)));
}

double CorrelationScore(double cross_energy, double reference_energy, double moving_energy)
{
    const double denominator = std::sqrt(reference_energy * moving_energy);
    if (denominator <= 0.0) {
        return 1.0;
    }

    const double correlation = std::clamp(cross_energy / denominator, -1.0, 1.0);
    return 1.0 - correlation;
}

double WindowWeight(int x, int y, int width, int height)
{
    if (width <= 1 || height <= 1) {
        return 1.0;
    }

    const double wx = 0.5 - 0.5 * std::cos((2.0 * kPi * (static_cast<double>(x) + 0.5)) / width);
    const double wy = 0.5 - 0.5 * std::cos((2.0 * kPi * (static_cast<double>(y) + 0.5)) / height);
    return std::max(0.01, wx * wy);
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

OffsetScore ScoreOffset(
    const ImageFrame& reference,
    const ImageFrame& moving,
    int dx,
    int dy,
    int min_overlap_pixels,
    int max_possible_overlap_pixels)
{
    int ref_x0 = 0;
    int ref_y0 = 0;
    int moving_x0 = 0;
    int moving_y0 = 0;
    int width = 0;
    int height = 0;
    if (!OverlapForOffset(reference, moving, dx, dy, ref_x0, ref_y0, moving_x0, moving_y0, width, height)) {
        return {};
    }

    const int overlap_pixels = width * height;
    if (overlap_pixels < min_overlap_pixels) {
        return {};
    }

    const int step = PixelStepForOverlap(width, height);
    int samples = 0;
    double reference_sum = 0.0;
    double moving_sum = 0.0;
    double reference_high_pass_sum = 0.0;
    double moving_high_pass_sum = 0.0;
    double weight_sum = 0.0;
    for (int y = 0; y < height; y += step) {
        for (int x = 0; x < width; x += step) {
            const int reference_x = ref_x0 + x;
            const int reference_y = ref_y0 + y;
            const int moving_x = moving_x0 + x;
            const int moving_y = moving_y0 + y;
            double reference_gx = 0.0;
            double reference_gy = 0.0;
            double moving_gx = 0.0;
            double moving_gy = 0.0;
            GradientAt(reference, reference_x, reference_y, reference_gx, reference_gy);
            GradientAt(moving, moving_x, moving_y, moving_gx, moving_gy);
            const double weight = StructuralSampleWeight(
                x,
                y,
                width,
                height,
                reference_gx,
                reference_gy,
                moving_gx,
                moving_gy);
            reference_sum += LuminanceAt(reference, reference_x, reference_y) * weight;
            moving_sum += LuminanceAt(moving, moving_x, moving_y) * weight;
            reference_high_pass_sum += HighPassAt(reference, reference_x, reference_y) * weight;
            moving_high_pass_sum += HighPassAt(moving, moving_x, moving_y) * weight;
            weight_sum += weight;
            ++samples;
        }
    }
    if (samples < kMinimumSamplesPerOffset || weight_sum <= 0.0) {
        return {};
    }

    const double reference_mean = reference_sum / weight_sum;
    const double moving_mean = moving_sum / weight_sum;
    const double reference_high_pass_mean = reference_high_pass_sum / weight_sum;
    const double moving_high_pass_mean = moving_high_pass_sum / weight_sum;
    double reference_luminance_energy = 0.0;
    double moving_luminance_energy = 0.0;
    double luminance_cross_energy = 0.0;
    double reference_high_pass_energy = 0.0;
    double moving_high_pass_energy = 0.0;
    double high_pass_cross_energy = 0.0;
    double reference_gradient_energy = 0.0;
    double moving_gradient_energy = 0.0;
    double gradient_cross_energy = 0.0;

    for (int y = 0; y < height; y += step) {
        for (int x = 0; x < width; x += step) {
            const int reference_x = ref_x0 + x;
            const int reference_y = ref_y0 + y;
            const int moving_x = moving_x0 + x;
            const int moving_y = moving_y0 + y;
            double reference_gx = 0.0;
            double reference_gy = 0.0;
            double moving_gx = 0.0;
            double moving_gy = 0.0;
            GradientAt(reference, reference_x, reference_y, reference_gx, reference_gy);
            GradientAt(moving, moving_x, moving_y, moving_gx, moving_gy);
            const double weight = StructuralSampleWeight(
                x,
                y,
                width,
                height,
                reference_gx,
                reference_gy,
                moving_gx,
                moving_gy);
            const double reference_value = LuminanceAt(reference, reference_x, reference_y) - reference_mean;
            const double moving_value = LuminanceAt(moving, moving_x, moving_y) - moving_mean;
            reference_luminance_energy += reference_value * reference_value * weight;
            moving_luminance_energy += moving_value * moving_value * weight;
            luminance_cross_energy += reference_value * moving_value * weight;
            const double reference_high_pass =
                HighPassAt(reference, reference_x, reference_y) - reference_high_pass_mean;
            const double moving_high_pass =
                HighPassAt(moving, moving_x, moving_y) - moving_high_pass_mean;
            reference_high_pass_energy += reference_high_pass * reference_high_pass * weight;
            moving_high_pass_energy += moving_high_pass * moving_high_pass * weight;
            high_pass_cross_energy += reference_high_pass * moving_high_pass * weight;
            reference_gradient_energy += (reference_gx * reference_gx + reference_gy * reference_gy) * weight;
            moving_gradient_energy += (moving_gx * moving_gx + moving_gy * moving_gy) * weight;
            gradient_cross_energy += (reference_gx * moving_gx + reference_gy * moving_gy) * weight;
        }
    }

    double combined_score = 0.0;
    double score_weight = 0.0;
    if (reference_luminance_energy >= kMinimumLuminanceEnergyPerSample * weight_sum &&
        moving_luminance_energy >= kMinimumLuminanceEnergyPerSample * weight_sum) {
        combined_score += CorrelationScore(
            luminance_cross_energy,
            reference_luminance_energy,
            moving_luminance_energy) * kLuminanceScoreWeight;
        score_weight += kLuminanceScoreWeight;
    }
    if (reference_high_pass_energy >= kMinimumHighPassEnergyPerSample * weight_sum &&
        moving_high_pass_energy >= kMinimumHighPassEnergyPerSample * weight_sum) {
        combined_score += CorrelationScore(
            high_pass_cross_energy,
            reference_high_pass_energy,
            moving_high_pass_energy) * kHighPassScoreWeight;
        score_weight += kHighPassScoreWeight;
    }
    if (reference_gradient_energy >= kMinimumGradientEnergyPerSample * weight_sum &&
        moving_gradient_energy >= kMinimumGradientEnergyPerSample * weight_sum) {
        combined_score += CorrelationScore(
            gradient_cross_energy,
            reference_gradient_energy,
            moving_gradient_energy) * kGradientScoreWeight;
        score_weight += kGradientScoreWeight;
    }
    if (score_weight <= 0.0) {
        return {};
    }

    const double overlap_ratio = std::clamp(
        static_cast<double>(overlap_pixels) / static_cast<double>(std::max(1, max_possible_overlap_pixels)),
        0.0,
        1.0);
    OffsetScore score;
    score.valid = true;
    score.error = (combined_score / score_weight) * kCorrelationScoreScale +
        (1.0 - overlap_ratio) * kOverlapPenaltyScale;
    score.overlap_pixels = overlap_pixels;
    return score;
}

bool IsBetterCandidate(const TranslationOffset& candidate, const TranslationOffset& current)
{
    return candidate.score < current.score ||
        (std::abs(candidate.score - current.score) <= 1e-9 && candidate.valid && !current.valid);
}

bool IsNearCandidate(
    const TranslationOffset& left,
    const TranslationOffset& right,
    int suppression_radius_x,
    int suppression_radius_y)
{
    return std::abs(left.dx - right.dx) <= suppression_radius_x &&
        std::abs(left.dy - right.dy) <= suppression_radius_y;
}

void InsertCandidate(
    std::vector<TranslationOffset>& candidates,
    TranslationOffset candidate,
    int max_candidates,
    int suppression_radius_x,
    int suppression_radius_y)
{
    if (!candidate.valid || max_candidates <= 0) {
        return;
    }

    for (TranslationOffset& existing : candidates) {
        if (IsNearCandidate(candidate, existing, suppression_radius_x, suppression_radius_y)) {
            if (IsBetterCandidate(candidate, existing)) {
                existing = candidate;
            }
            std::sort(
                candidates.begin(),
                candidates.end(),
                [](const TranslationOffset& left, const TranslationOffset& right) {
                    return left.score < right.score;
                });
            return;
        }
    }

    candidates.push_back(candidate);
    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const TranslationOffset& left, const TranslationOffset& right) {
            return left.score < right.score;
        });
    if (static_cast<int>(candidates.size()) > max_candidates) {
        candidates.resize(static_cast<std::size_t>(max_candidates));
    }
}

int NextPowerOfTwo(int value)
{
    int result = 1;
    while (result < value) {
        result <<= 1;
    }
    return result;
}

void Fft1D(std::vector<Complex>& values, bool inverse)
{
    const int count = static_cast<int>(values.size());
    for (int index = 1, swap_index = 0; index < count; ++index) {
        int bit = count >> 1;
        for (; swap_index & bit; bit >>= 1) {
            swap_index ^= bit;
        }
        swap_index ^= bit;
        if (index < swap_index) {
            std::swap(values[index], values[swap_index]);
        }
    }

    for (int length = 2; length <= count; length <<= 1) {
        const double angle = (inverse ? 2.0 : -2.0) * kPi / static_cast<double>(length);
        const Complex step(std::cos(angle), std::sin(angle));
        for (int start = 0; start < count; start += length) {
            Complex phase(1.0, 0.0);
            const int half_length = length >> 1;
            for (int offset = 0; offset < half_length; ++offset) {
                const Complex even = values[start + offset];
                const Complex odd = values[start + offset + half_length] * phase;
                values[start + offset] = even + odd;
                values[start + offset + half_length] = even - odd;
                phase *= step;
            }
        }
    }

    if (inverse && count > 0) {
        const double scale = 1.0 / static_cast<double>(count);
        for (Complex& value : values) {
            value *= scale;
        }
    }
}

void Fft2D(std::vector<Complex>& values, int width, int height, bool inverse)
{
    std::vector<Complex> line(static_cast<std::size_t>(std::max(width, height)));
    for (int y = 0; y < height; ++y) {
        std::copy(
            values.begin() + static_cast<std::ptrdiff_t>(y) * width,
            values.begin() + static_cast<std::ptrdiff_t>(y + 1) * width,
            line.begin());
        line.resize(static_cast<std::size_t>(width));
        Fft1D(line, inverse);
        std::copy(
            line.begin(),
            line.end(),
            values.begin() + static_cast<std::ptrdiff_t>(y) * width);
        line.resize(static_cast<std::size_t>(std::max(width, height)));
    }

    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            line[static_cast<std::size_t>(y)] =
                values[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                       static_cast<std::size_t>(x)];
        }
        line.resize(static_cast<std::size_t>(height));
        Fft1D(line, inverse);
        for (int y = 0; y < height; ++y) {
            values[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                   static_cast<std::size_t>(x)] = line[static_cast<std::size_t>(y)];
        }
        line.resize(static_cast<std::size_t>(std::max(width, height)));
    }
}

double FillPhaseInput(const ImageFrame& frame, int transform_width, std::vector<Complex>& signal)
{
    double sum = 0.0;
    int samples = 0;
    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            sum += HighPassAt(frame, x, y);
            ++samples;
        }
    }
    if (samples <= 0) {
        return 0.0;
    }

    const double mean = sum / static_cast<double>(samples);
    double energy = 0.0;
    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            const double value =
                (HighPassAt(frame, x, y) - mean) * WindowWeight(x, y, frame.width, frame.height);
            signal[static_cast<std::size_t>(y) * static_cast<std::size_t>(transform_width) +
                   static_cast<std::size_t>(x)] = Complex(value, 0.0);
            energy += value * value;
        }
    }
    return energy / static_cast<double>(samples);
}

std::vector<TranslationOffset> EstimatePhaseCorrelationCandidatesInRange(
    const ImageFrame& reference,
    const ImageFrame& moving,
    int min_dx,
    int max_dx,
    int min_dy,
    int max_dy,
    int max_candidates,
    int suppression_radius_x,
    int suppression_radius_y,
    int min_overlap_pixels,
    int max_possible_overlap_pixels)
{
    const int transform_width = NextPowerOfTwo(reference.width + moving.width);
    const int transform_height = NextPowerOfTwo(reference.height + moving.height);
    const long long transform_pixels =
        static_cast<long long>(transform_width) * static_cast<long long>(transform_height);
    if (transform_pixels <= 0 || transform_pixels > kMaxPhaseCorrelationPixels) {
        return {};
    }

    std::vector<Complex> reference_signal(static_cast<std::size_t>(transform_pixels));
    std::vector<Complex> moving_signal(static_cast<std::size_t>(transform_pixels));
    const double reference_energy = FillPhaseInput(reference, transform_width, reference_signal);
    const double moving_energy = FillPhaseInput(moving, transform_width, moving_signal);
    if (reference_energy < kMinimumPhaseInputEnergy || moving_energy < kMinimumPhaseInputEnergy) {
        return {};
    }

    Fft2D(reference_signal, transform_width, transform_height, false);
    Fft2D(moving_signal, transform_width, transform_height, false);
    for (std::size_t index = 0; index < reference_signal.size(); ++index) {
        const Complex cross_power = reference_signal[index] * std::conj(moving_signal[index]);
        const double magnitude = std::abs(cross_power);
        reference_signal[index] =
            magnitude > kPhaseMagnitudeEpsilon ? cross_power / magnitude : Complex(0.0, 0.0);
    }
    Fft2D(reference_signal, transform_width, transform_height, true);

    std::vector<TranslationOffset> raw_peaks;
    const int raw_candidate_limit = std::max(kMaxPhaseCorrelationCandidates, max_candidates * 2);
    for (int y = 0; y < transform_height; ++y) {
        const int dy = y <= transform_height / 2 ? y : y - transform_height;
        if (dy < min_dy || dy > max_dy) {
            continue;
        }
        for (int x = 0; x < transform_width; ++x) {
            const int dx = x <= transform_width / 2 ? x : x - transform_width;
            if (dx < min_dx || dx > max_dx) {
                continue;
            }

            const double peak =
                reference_signal[static_cast<std::size_t>(y) * static_cast<std::size_t>(transform_width) +
                                 static_cast<std::size_t>(x)]
                    .real();
            if (peak <= 0.0 ||
                (static_cast<int>(raw_peaks.size()) >= raw_candidate_limit &&
                 -peak >= raw_peaks.back().score)) {
                continue;
            }

            int ref_x0 = 0;
            int ref_y0 = 0;
            int moving_x0 = 0;
            int moving_y0 = 0;
            int overlap_width = 0;
            int overlap_height = 0;
            if (!OverlapForOffset(
                    reference,
                    moving,
                    dx,
                    dy,
                    ref_x0,
                    ref_y0,
                    moving_x0,
                    moving_y0,
                    overlap_width,
                    overlap_height) ||
                overlap_width * overlap_height < min_overlap_pixels) {
                continue;
            }

            TranslationOffset candidate;
            candidate.dx = dx;
            candidate.dy = dy;
            candidate.score = -peak;
            candidate.confidence = std::clamp(peak * 8.0, 0.0, 1.0);
            candidate.overlap_pixels = overlap_width * overlap_height;
            candidate.valid = true;
            InsertCandidate(
                raw_peaks,
                candidate,
                raw_candidate_limit,
                std::max(1, suppression_radius_x * 2),
                std::max(1, suppression_radius_y * 2));
        }
    }

    std::vector<TranslationOffset> candidates;
    for (const TranslationOffset& peak : raw_peaks) {
        const OffsetScore score = ScoreOffset(
            reference,
            moving,
            peak.dx,
            peak.dy,
            min_overlap_pixels,
            max_possible_overlap_pixels);
        if (!score.valid) {
            continue;
        }

        TranslationOffset candidate = peak;
        candidate.score = score.error;
        candidate.overlap_pixels = score.overlap_pixels;
        InsertCandidate(
            candidates,
            candidate,
            max_candidates,
            suppression_radius_x,
            suppression_radius_y);
    }
    return candidates;
}

std::vector<TranslationOffset> EstimateTranslationCandidatesInRange(
    const ImageFrame& reference,
    const ImageFrame& moving,
    int min_dx,
    int max_dx,
    int min_dy,
    int max_dy,
    int max_candidates,
    int suppression_radius_x,
    int suppression_radius_y,
    int offset_step_x,
    int offset_step_y)
{
    if (!HasReadablePixels(reference) || !HasReadablePixels(moving) || min_dx > max_dx || min_dy > max_dy) {
        return {};
    }

    offset_step_x = std::max(1, offset_step_x);
    offset_step_y = std::max(1, offset_step_y);
    std::vector<TranslationOffset> candidates;
    const int max_possible_overlap_pixels = std::min(reference.width * reference.height, moving.width * moving.height);
    const int min_overlap_pixels = std::max(
        16,
        static_cast<int>(std::ceil(max_possible_overlap_pixels * kMinimumOverlapAreaFraction)));
    const long long search_positions =
        (static_cast<long long>(max_dx - min_dx) / offset_step_x + 1LL) *
        (static_cast<long long>(max_dy - min_dy) / offset_step_y + 1LL);
    const bool use_phase_candidates =
        max_candidates > 1 && search_positions >= kMinimumPhaseCorrelationSearchPositions;
    const int scoring_step_x = use_phase_candidates ? std::max(offset_step_x, 2) : offset_step_x;
    const int scoring_step_y = use_phase_candidates ? std::max(offset_step_y, 2) : offset_step_y;

    for (int dy = min_dy; dy <= max_dy; dy += scoring_step_y) {
        for (int dx = min_dx; dx <= max_dx; dx += scoring_step_x) {
            const OffsetScore score = ScoreOffset(
                reference,
                moving,
                dx,
                dy,
                min_overlap_pixels,
                max_possible_overlap_pixels);
            if (!score.valid) {
                continue;
            }

            TranslationOffset candidate;
            candidate.dx = dx;
            candidate.dy = dy;
            candidate.score = score.error;
            candidate.overlap_pixels = score.overlap_pixels;
            candidate.valid = true;
            InsertCandidate(
                candidates,
                candidate,
                max_candidates,
                suppression_radius_x,
                suppression_radius_y);
        }
    }

    if (use_phase_candidates) {
        const std::vector<TranslationOffset> phase_candidates = EstimatePhaseCorrelationCandidatesInRange(
            reference,
            moving,
            min_dx,
            max_dx,
            min_dy,
            max_dy,
            max_candidates,
            suppression_radius_x,
            suppression_radius_y,
            min_overlap_pixels,
            max_possible_overlap_pixels);
        for (const TranslationOffset& phase_candidate : phase_candidates) {
            InsertCandidate(
                candidates,
                phase_candidate,
                max_candidates,
                suppression_radius_x,
                suppression_radius_y);
        }
    }

    for (std::size_t index = 0; index < candidates.size(); ++index) {
        double nearest_competing_score = std::numeric_limits<double>::infinity();
        for (std::size_t other_index = 0; other_index < candidates.size(); ++other_index) {
            if (index == other_index) {
                continue;
            }
            nearest_competing_score = std::min(nearest_competing_score, candidates[other_index].score);
        }
        if (!std::isfinite(nearest_competing_score)) {
            candidates[index].confidence = 1.0;
        } else {
            candidates[index].confidence = std::clamp(
                (nearest_competing_score - candidates[index].score) /
                    std::max(0.25, std::abs(candidates[index].score)),
                0.0,
                1.0);
        }
    }

    return candidates;
}

TranslationOffset EstimateTranslationInRange(
    const ImageFrame& reference,
    const ImageFrame& moving,
    int min_dx,
    int max_dx,
    int min_dy,
    int max_dy)
{
    const std::vector<TranslationOffset> candidates = EstimateTranslationCandidatesInRange(
        reference,
        moving,
        min_dx,
        max_dx,
        min_dy,
        max_dy,
        1,
        0,
        0,
        1,
        1);
    return candidates.empty() ? TranslationOffset{} : candidates.front();
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

std::vector<TranslationOffset> ImageRegistration::EstimateTranslationCandidates(
    const ImageFrame& reference,
    const ImageFrame& moving,
    int max_shift_x,
    int max_shift_y,
    int max_candidates,
    int offset_step_x,
    int offset_step_y)
{
    if (!reference.IsValid() || !moving.IsValid() || max_shift_x < 0 || max_shift_y < 0 || max_candidates <= 0) {
        return {};
    }

    const int suppression_radius_x = 1;
    const int suppression_radius_y = 1;
    return EstimateTranslationCandidatesInRange(
        reference,
        moving,
        -max_shift_x,
        max_shift_x,
        -max_shift_y,
        max_shift_y,
        max_candidates,
        suppression_radius_x,
        suppression_radius_y,
        offset_step_x,
        offset_step_y);
}

std::vector<TranslationOffset> ImageRegistration::EstimateTranslationCandidates(
    const ImageFrame& reference,
    const ImageFrame& moving,
    int min_dx,
    int max_dx,
    int min_dy,
    int max_dy,
    int max_candidates,
    int offset_step_x,
    int offset_step_y)
{
    if (!reference.IsValid() || !moving.IsValid() || min_dx > max_dx || min_dy > max_dy || max_candidates <= 0) {
        return {};
    }

    const int suppression_radius_x = 1;
    const int suppression_radius_y = 1;
    return EstimateTranslationCandidatesInRange(
        reference,
        moving,
        min_dx,
        max_dx,
        min_dy,
        max_dy,
        max_candidates,
        suppression_radius_x,
        suppression_radius_y,
        offset_step_x,
        offset_step_y);
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
