#include "PreviewDisplayActions.h"

#include "PreviewFrameComposer.h"

namespace {

bool HasUsableFluorescenceChannel(const std::vector<FluorescenceChannel>& fluorescence_channels)
{
    for (const FluorescenceChannel& channel : fluorescence_channels) {
        if (channel.visible && channel.frame.IsValid()) {
            return true;
        }
    }
    return false;
}

} // namespace

std::vector<std::wstring> PreviewDisplayActions::PseudoColorLabels()
{
    std::vector<std::wstring> labels;
    const std::vector<PseudoColorPalette>& palettes = PseudoColorMapper::PaletteOptions();
    labels.reserve(palettes.size());
    for (PseudoColorPalette palette : palettes) {
        labels.push_back(PseudoColorMapper::Label(palette));
    }
    return labels;
}

PseudoColorSelectionResult PreviewDisplayActions::SelectPseudoColor(int selection)
{
    PseudoColorSelectionResult result;
    result.palette = PseudoColorMapper::PaletteAtIndex(selection);
    result.message = L"Pseudo color: " + PseudoColorMapper::Label(result.palette);
    return result;
}

ImageFrame PreviewDisplayActions::BuildPreviewFrame(
    const ImageFrame& source,
    const ProcessingResultFrames& processing_frames,
    const std::vector<FluorescenceChannel>& fluorescence_channels,
    bool show_fusion_preview,
    PseudoColorPalette pseudo_color_palette)
{
    PreviewFrameComposition composition;
    composition.source = &source;
    composition.processing_result = &processing_frames.ProcessingResult();
    composition.fluorescence_channels = &fluorescence_channels;
    composition.show_processing_result = processing_frames.IsProcessingResultVisible();
    composition.show_fusion_preview = show_fusion_preview;
    composition.pseudo_color_palette = pseudo_color_palette;
    return PreviewFrameComposer::Compose(composition);
}

std::wstring PreviewDisplayActions::PreviewModeLabel(
    const ImageFrame& source,
    const ProcessingResultFrames& processing_frames,
    const std::vector<FluorescenceChannel>& fluorescence_channels,
    bool show_fusion_preview,
    PseudoColorPalette pseudo_color_palette)
{
    if (processing_frames.IsProcessingResultVisible()) {
        return L"Processing result: " + processing_frames.DisplaySourceLabel();
    }

    if (show_fusion_preview && HasUsableFluorescenceChannel(fluorescence_channels)) {
        return L"Fluorescence fusion";
    }

    if (source.IsValid() && pseudo_color_palette != PseudoColorPalette::Original) {
        return L"Pseudo color: " + PseudoColorMapper::Label(pseudo_color_palette);
    }

    return source.IsValid() ? L"Original image" : L"(none)";
}
