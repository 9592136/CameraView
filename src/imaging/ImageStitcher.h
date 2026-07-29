#pragma once

#include "../domain/ImageFrame.h"

#include <atomic>
#include <functional>
#include <vector>

enum class StitchRegistrationMethod {
    Phase,
    Feature,
    Auto,
    Sift,
    Micro
};

enum class StitchTransformModel {
    Translation,
    Affine,
    Homography
};

enum class StitchBlendMode {
    Linear,
    None
};

enum class StitchLayoutMode {
    Grid,
    Linear
};

struct StitchTile {
    ImageFrame frame;
    int offset_x = 0;
    int offset_y = 0;
    bool estimated_position = false;
};

struct StitchOptimizationOptions {
    int search_radius_x = 16;
    int search_radius_y = 16;
    int iterations = 25;
    bool use_orb_registration = true;
    StitchRegistrationMethod registration_method = StitchRegistrationMethod::Auto;
};

struct StitchProcessingOptions {
    int overlap_percent = 25;
    StitchLayoutMode layout_mode = StitchLayoutMode::Grid;
    int grid_rows = 3;
    int grid_cols = 4;
    StitchRegistrationMethod registration_method = StitchRegistrationMethod::Micro;
    StitchTransformModel transform_model = StitchTransformModel::Affine;
    StitchBlendMode blend_mode = StitchBlendMode::Linear;
};

struct StitchOptimizationResult {
    std::vector<StitchTile> tiles;
    int constraint_count = 0;
    bool optimized = false;
};

class ImageStitcher {
public:
    static StitchOptimizationResult OptimizeTileOffsets(
        const std::vector<StitchTile>& tiles,
        StitchOptimizationOptions options = {},
        const std::atomic_bool* cancel_requested = nullptr,
        const std::function<void(int)>& progress_callback = {});

    static ImageFrame StitchAverage(
        const std::vector<StitchTile>& tiles,
        StitchBlendMode blend_mode = StitchBlendMode::Linear,
        const std::atomic_bool* cancel_requested = nullptr,
        const std::function<void(int)>& progress_callback = {});
};
