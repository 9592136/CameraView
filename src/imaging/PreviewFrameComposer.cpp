#include "PreviewFrameComposer.h"

#include "ChannelFusionEngine.h"

ImageFrame PreviewFrameComposer::Compose(const PreviewFrameComposition& composition)
{
    if (composition.show_processing_result &&
        composition.processing_result &&
        composition.processing_result->IsValid()) {
        return *composition.processing_result;
    }

    if (composition.show_fusion_preview &&
        composition.fluorescence_channels &&
        !composition.fluorescence_channels->empty()) {
        ImageFrame fused = ChannelFusionEngine::Fuse(*composition.fluorescence_channels);
        if (fused.IsValid()) {
            return fused;
        }
    }

    if (!composition.source || !composition.source->IsValid()) {
        return {};
    }

    if (composition.pseudo_color_palette == PseudoColorPalette::Original) {
        return *composition.source;
    }

    return PseudoColorMapper::Apply(*composition.source, composition.pseudo_color_palette);
}
