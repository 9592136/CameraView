#include "ProcessingParameterRules.h"

#include <algorithm>

namespace {

constexpr int kMinStitchSearchPercent = 5;
constexpr int kMaxStitchSearchPercent = 100;
constexpr int kDefaultStitchSearchPercent = 50;
constexpr int kMinEdfFocusRadius = 1;
constexpr int kMaxEdfFocusRadius = 16;
constexpr int kMinRegistrationSearchRadius = 8;
constexpr int kMinStitchOptimizationSearchRadius = 1;
constexpr int kMaxStitchOptimizationSearchRadius = 128;
constexpr int kStitchOptimizationIterations = 30;

} // namespace

int ProcessingParameterRules::MinStitchSearchPercent()
{
    return kMinStitchSearchPercent;
}

int ProcessingParameterRules::MaxStitchSearchPercent()
{
    return kMaxStitchSearchPercent;
}

int ProcessingParameterRules::DefaultStitchSearchPercent()
{
    return kDefaultStitchSearchPercent;
}

bool ProcessingParameterRules::IsValidStitchSearchPercent(int percent)
{
    return percent >= kMinStitchSearchPercent && percent <= kMaxStitchSearchPercent;
}

int ProcessingParameterRules::ClampStitchSearchPercent(int percent)
{
    return std::clamp(percent, kMinStitchSearchPercent, kMaxStitchSearchPercent);
}

int ProcessingParameterRules::MinEdfFocusRadius()
{
    return kMinEdfFocusRadius;
}

int ProcessingParameterRules::MaxEdfFocusRadius()
{
    return kMaxEdfFocusRadius;
}

EdfOptions ProcessingParameterRules::DefaultEdfOptions()
{
    return EdfOptions();
}

bool ProcessingParameterRules::IsValidEdfFocusRadius(int radius)
{
    return radius >= kMinEdfFocusRadius && radius <= kMaxEdfFocusRadius;
}

int ProcessingParameterRules::ClampEdfFocusRadius(int radius)
{
    return std::clamp(radius, kMinEdfFocusRadius, kMaxEdfFocusRadius);
}

EdfOptions ProcessingParameterRules::NormalizeEdfOptions(EdfOptions options)
{
    options.focus_radius = ClampEdfFocusRadius(options.focus_radius);
    return options;
}

StitchSearchRadius ProcessingParameterRules::RegistrationSearchRadius(
    int width,
    int height,
    int percent)
{
    const int normalized_percent = ClampStitchSearchPercent(percent);
    StitchSearchRadius radius;
    radius.x = std::max(kMinRegistrationSearchRadius, (std::max(0, width) * normalized_percent) / 100);
    radius.y = std::max(kMinRegistrationSearchRadius, (std::max(0, height) * normalized_percent) / 100);
    return radius;
}

StitchOptimizationOptions ProcessingParameterRules::StitchOptimizationOptionsFor(
    const std::vector<StitchTile>& tiles,
    int percent)
{
    int max_width = 1;
    int max_height = 1;
    for (const StitchTile& tile : tiles) {
        if (tile.frame.IsValid()) {
            max_width = std::max(max_width, tile.frame.width);
            max_height = std::max(max_height, tile.frame.height);
        }
    }

    const int normalized_percent = ClampStitchSearchPercent(percent);
    StitchOptimizationOptions options;
    options.search_radius_x = std::clamp(
        (max_width * normalized_percent) / 100,
        kMinStitchOptimizationSearchRadius,
        kMaxStitchOptimizationSearchRadius);
    options.search_radius_y = std::clamp(
        (max_height * normalized_percent) / 100,
        kMinStitchOptimizationSearchRadius,
        kMaxStitchOptimizationSearchRadius);
    options.iterations = kStitchOptimizationIterations;
    return options;
}
