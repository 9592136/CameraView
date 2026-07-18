#pragma once

#include "../domain/ImageFrame.h"

#include <vector>

struct TranslationOffset {
    int dx = 0;
    int dy = 0;
    double score = 0.0;
    double confidence = 0.0;
    int overlap_pixels = 0;
    bool valid = false;
};

class ImageRegistration {
public:
    static TranslationOffset EstimateTranslation(
        const ImageFrame& reference,
        const ImageFrame& moving,
        int max_shift_x,
        int max_shift_y);

    static std::vector<TranslationOffset> EstimateTranslationCandidates(
        const ImageFrame& reference,
        const ImageFrame& moving,
        int max_shift_x,
        int max_shift_y,
        int max_candidates,
        int offset_step_x = 1,
        int offset_step_y = 1);

    static std::vector<TranslationOffset> EstimateTranslationCandidates(
        const ImageFrame& reference,
        const ImageFrame& moving,
        int min_dx,
        int max_dx,
        int min_dy,
        int max_dy,
        int max_candidates,
        int offset_step_x = 1,
        int offset_step_y = 1);

    static TranslationOffset RefineTranslation(
        const ImageFrame& reference,
        const ImageFrame& moving,
        int initial_dx,
        int initial_dy,
        int search_radius_x,
        int search_radius_y);
};
