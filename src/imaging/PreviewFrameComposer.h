#pragma once

#include "../domain/ImageFrame.h"
#include "Fluorescence.h"
#include "PseudoColorMapper.h"

#include <vector>

struct PreviewFrameComposition {
    const ImageFrame* source = nullptr;
    const ImageFrame* processing_result = nullptr;
    const std::vector<FluorescenceChannel>* fluorescence_channels = nullptr;
    bool show_processing_result = false;
    bool show_fusion_preview = false;
    PseudoColorPalette pseudo_color_palette = PseudoColorPalette::Original;
};

class PreviewFrameComposer {
public:
    static ImageFrame Compose(const PreviewFrameComposition& composition);
};
