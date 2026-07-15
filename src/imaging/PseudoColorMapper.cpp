#include "PseudoColorMapper.h"

#include <algorithm>

namespace {

struct BgrColor {
    unsigned char b = 0;
    unsigned char g = 0;
    unsigned char r = 0;
};

unsigned char Luminance(const unsigned char* pixel)
{
    const int blue = pixel[0];
    const int green = pixel[1];
    const int red = pixel[2];
    return static_cast<unsigned char>((red * 77 + green * 150 + blue * 29) >> 8);
}

BgrColor MapHot(unsigned char value)
{
    if (value < 85) {
        return BgrColor{0, 0, static_cast<unsigned char>(value * 3)};
    }
    if (value < 170) {
        return BgrColor{0, static_cast<unsigned char>((value - 85) * 3), 255};
    }
    return BgrColor{static_cast<unsigned char>(std::min(255, (value - 170) * 3)), 255, 255};
}

BgrColor MapColor(unsigned char value, PseudoColorPalette palette)
{
    switch (palette) {
    case PseudoColorPalette::Grayscale:
        return BgrColor{value, value, value};
    case PseudoColorPalette::Hot:
        return MapHot(value);
    case PseudoColorPalette::Green:
        return BgrColor{0, value, 0};
    case PseudoColorPalette::Cyan:
        return BgrColor{value, value, 0};
    case PseudoColorPalette::Magenta:
        return BgrColor{value, 0, value};
    case PseudoColorPalette::Original:
    default:
        return BgrColor{value, value, value};
    }
}

} // namespace

const std::vector<PseudoColorPalette>& PseudoColorMapper::PaletteOptions()
{
    static const std::vector<PseudoColorPalette> palettes = {
        PseudoColorPalette::Original,
        PseudoColorPalette::Grayscale,
        PseudoColorPalette::Hot,
        PseudoColorPalette::Green,
        PseudoColorPalette::Cyan,
        PseudoColorPalette::Magenta
    };
    return palettes;
}

PseudoColorPalette PseudoColorMapper::PaletteAtIndex(int index)
{
    const std::vector<PseudoColorPalette>& palettes = PaletteOptions();
    if (index < 0 || index >= static_cast<int>(palettes.size())) {
        return PseudoColorPalette::Original;
    }
    return palettes[static_cast<std::size_t>(index)];
}

ImageFrame PseudoColorMapper::Apply(const ImageFrame& source, PseudoColorPalette palette)
{
    if (palette == PseudoColorPalette::Original || !source.IsValid()) {
        return source;
    }

    ImageFrame output;
    output.width = source.width;
    output.height = source.height;
    output.stride = source.stride;
    output.timestamp = source.timestamp;
    output.sequence = source.sequence;
    output.bgr.assign(source.bgr.size(), 0);

    for (int y = 0; y < source.height; ++y) {
        const unsigned char* src = source.bgr.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(source.stride);
        unsigned char* dst = output.bgr.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(output.stride);
        for (int x = 0; x < source.width; ++x) {
            const unsigned char value = Luminance(src + x * 3);
            const BgrColor color = MapColor(value, palette);
            dst[x * 3 + 0] = color.b;
            dst[x * 3 + 1] = color.g;
            dst[x * 3 + 2] = color.r;
        }
    }

    return output;
}

std::wstring PseudoColorMapper::Label(PseudoColorPalette palette)
{
    switch (palette) {
    case PseudoColorPalette::Original:
        return L"Original";
    case PseudoColorPalette::Grayscale:
        return L"Grayscale";
    case PseudoColorPalette::Hot:
        return L"Hot";
    case PseudoColorPalette::Green:
        return L"Green";
    case PseudoColorPalette::Cyan:
        return L"Cyan";
    case PseudoColorPalette::Magenta:
        return L"Magenta";
    default:
        return L"";
    }
}
