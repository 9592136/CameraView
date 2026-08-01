#include "PreviewFrameComposer.h"

#include "ChannelFusionEngine.h"

void PreviewFrameComposer::ComposeInto(const PreviewFrameComposition& composition, ImageFrame& out)
{
    if (composition.show_processing_result &&
        composition.processing_result &&
        composition.processing_result->IsValid()) {
        out = *composition.processing_result;
        return;
    }

    if (composition.show_fusion_preview &&
        composition.fluorescence_channels &&
        !composition.fluorescence_channels->empty()) {
        ImageFrame fused = ChannelFusionEngine::Fuse(*composition.fluorescence_channels);
        if (fused.IsValid()) {
            out = std::move(fused);
            return;
        }
    }

    if (!composition.source || !composition.source->IsValid()) {
        out = {};
        return;
    }

    if (composition.pseudo_color_palette == PseudoColorPalette::Original) {
        out = *composition.source;
        return;
    }

    ImageFrame colored = PseudoColorMapper::Apply(*composition.source, composition.pseudo_color_palette);
    out = std::move(colored);
}

ImageFrame PreviewFrameComposer::Compose(const PreviewFrameComposition& composition)
{
    ImageFrame result;
    ComposeInto(composition, result);
    return result;
}
