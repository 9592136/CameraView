#pragma once

#include "../domain/ImageFrame.h"

#include <string>

struct RgbColor {
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
};

struct DyeProfile {
    std::wstring name;
    double excitation_nm = 0.0;
    double emission_nm = 0.0;
    RgbColor color;
};

struct FluorescenceChannelRecipe {
    std::wstring name;
    RgbColor color;
    double exposure_ms = 0.0;
    bool visible = true;
    unsigned char black_level = 0;
    unsigned char white_level = 255;
};

struct FluorescenceChannel {
    std::wstring name;
    ImageFrame frame;
    RgbColor color;
    double exposure_ms = 0.0;
    bool visible = true;
    unsigned char black_level = 0;
    unsigned char white_level = 255;
};

struct FluorescenceCapturePreset {
    std::wstring dye_name;
    double exposure_ms = 10.0;
    RgbColor color;
};
