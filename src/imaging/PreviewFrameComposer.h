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
    /// Composes a preview frame, returning a new ImageFrame.
    /// Convenience wrapper that delegates to ComposeInto.
    static ImageFrame Compose(const PreviewFrameComposition& composition);

    /// Composes the preview directly into @p out, reusing its buffer where possible.
    /// When only the source frame is shown without modification the source content
    /// is assigned to @p out (single copy); for pseudo-color / fusion / processing
    /// result the frame is moved into @p out, avoiding an intermediate temporary.
    static void ComposeInto(const PreviewFrameComposition& composition, ImageFrame& out);
};
