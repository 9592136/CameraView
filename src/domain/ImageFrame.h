#pragma once

#include <vector>

struct ImageFrame {
    int width = 0;
    int height = 0;
    int stride = 0;
    unsigned long timestamp = 0;
    unsigned long long sequence = 0;
    std::vector<unsigned char> bgr;

    bool IsValid() const
    {
        return width > 0 && height > 0 && stride > 0 && !bgr.empty();
    }
};
