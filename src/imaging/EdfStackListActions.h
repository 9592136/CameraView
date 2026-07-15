#pragma once

#include "../domain/ImageFrame.h"

#include <cstddef>
#include <string>
#include <vector>

enum class EdfStackListActionStatus {
    NoFrame,
    Added
};

struct EdfStackListActionResult {
    EdfStackListActionStatus status = EdfStackListActionStatus::NoFrame;
    bool changed = false;
    std::size_t frame_count = 0;
    std::wstring message;
};

class EdfStackListActions {
public:
    static EdfStackListActionResult AddCurrentFrame(
        std::vector<ImageFrame>& stack,
        ImageFrame frame);
};
