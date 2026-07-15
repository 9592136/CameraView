#pragma once

#include "../domain/ImageFrame.h"
#include "Fluorescence.h"
#include "ProcessingResultFrames.h"
#include "PseudoColorMapper.h"

#include <string>
#include <vector>

struct PseudoColorSelectionResult {
    PseudoColorPalette palette = PseudoColorPalette::Original;
    std::wstring message;
};

class PreviewDisplayActions {
public:
    static std::vector<std::wstring> PseudoColorLabels();
    static PseudoColorSelectionResult SelectPseudoColor(int selection);

    static ImageFrame BuildPreviewFrame(
        const ImageFrame& source,
        const ProcessingResultFrames& processing_frames,
        const std::vector<FluorescenceChannel>& fluorescence_channels,
        bool show_fusion_preview,
        PseudoColorPalette pseudo_color_palette);

    static std::wstring PreviewModeLabel(
        const ImageFrame& source,
        const ProcessingResultFrames& processing_frames,
        const std::vector<FluorescenceChannel>& fluorescence_channels,
        bool show_fusion_preview,
        PseudoColorPalette pseudo_color_palette);
};
