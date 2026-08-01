#pragma once

#include <cstdint>
#include <vector>

struct ImageFrame {
    int width = 0;
    int height = 0;
    int stride = 0;
    uint32_t timestamp = 0;
    uint64_t sequence = 0;
    std::vector<unsigned char> bgr;

    bool IsValid() const
    {
        return width > 0 && height > 0 && stride > 0 && !bgr.empty();
    }
};
