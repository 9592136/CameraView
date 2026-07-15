#pragma once

#include "../domain/ImageFrame.h"

struct TranslationOffset {
    int dx = 0;
    int dy = 0;
    double score = 0.0;
    bool valid = false;
};

class ImageRegistration {
public:
    static TranslationOffset EstimateTranslation(
        const ImageFrame& reference,
        const ImageFrame& moving,
        int max_shift_x,
        int max_shift_y);

    static TranslationOffset RefineTranslation(
        const ImageFrame& reference,
        const ImageFrame& moving,
        int initial_dx,
        int initial_dy,
        int search_radius_x,
        int search_radius_y);
};
