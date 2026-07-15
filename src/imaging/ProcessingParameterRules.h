#pragma once

#include "EdfProcessor.h"
#include "ImageStitcher.h"

#include <vector>

struct StitchSearchRadius {
    int x = 1;
    int y = 1;
};

class ProcessingParameterRules {
public:
    static int MinStitchSearchPercent();
    static int MaxStitchSearchPercent();
    static int DefaultStitchSearchPercent();
    static bool IsValidStitchSearchPercent(int percent);
    static int ClampStitchSearchPercent(int percent);

    static int MinEdfFocusRadius();
    static int MaxEdfFocusRadius();
    static EdfOptions DefaultEdfOptions();
    static bool IsValidEdfFocusRadius(int radius);
    static int ClampEdfFocusRadius(int radius);
    static EdfOptions NormalizeEdfOptions(EdfOptions options);

    static StitchSearchRadius RegistrationSearchRadius(int width, int height, int percent);
    static StitchOptimizationOptions StitchOptimizationOptionsFor(
        const std::vector<StitchTile>& tiles,
        int percent);
};
