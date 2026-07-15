#pragma once

#include "../domain/ImageFrame.h"

#include <atomic>
#include <functional>
#include <vector>

struct EdfResult {
    ImageFrame composite_frame;
    ImageFrame focus_map;
};

struct EdfOptions {
    int focus_radius = 1;
};

class EdfProcessor {
public:
    static EdfResult ComposeFocusStack(
        const std::vector<ImageFrame>& frames,
        const EdfOptions& options,
        const std::atomic_bool* cancel_requested = nullptr,
        const std::function<void(int)>& progress_callback = {});
    static EdfResult ComposeFocusStack(
        const std::vector<ImageFrame>& frames,
        const std::atomic_bool* cancel_requested = nullptr,
        const std::function<void(int)>& progress_callback = {});
    static ImageFrame FuseFocusStack(
        const std::vector<ImageFrame>& frames,
        const EdfOptions& options,
        const std::atomic_bool* cancel_requested = nullptr,
        const std::function<void(int)>& progress_callback = {});
    static ImageFrame FuseFocusStack(
        const std::vector<ImageFrame>& frames,
        const std::atomic_bool* cancel_requested = nullptr,
        const std::function<void(int)>& progress_callback = {});
};
