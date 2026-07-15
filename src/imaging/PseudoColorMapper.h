#pragma once

#include "../domain/ImageFrame.h"

#include <vector>
#include <string>

enum class PseudoColorPalette {
    Original,
    Grayscale,
    Hot,
    Green,
    Cyan,
    Magenta
};

class PseudoColorMapper {
public:
    static const std::vector<PseudoColorPalette>& PaletteOptions();
    static PseudoColorPalette PaletteAtIndex(int index);
    static ImageFrame Apply(const ImageFrame& source, PseudoColorPalette palette);
    static std::wstring Label(PseudoColorPalette palette);
};
