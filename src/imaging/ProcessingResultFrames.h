#pragma once

#include "../domain/ImageFrame.h"
#include "ProcessingJobState.h"

#include <string>

enum class ProcessingResultDisplaySource {
    None,
    Stitch,
    EdfComposite,
    EdfFocusMap
};

class ProcessingResultFrames {
public:
    const ImageFrame& ProcessingResult() const { return processing_result_; }
    const ImageFrame& EdfCompositeFrame() const { return edf_composite_frame_; }
    const ImageFrame& EdfFocusMap() const { return edf_focus_map_; }
    bool IsProcessingResultVisible() const { return show_processing_result_ && processing_result_.IsValid(); }
    ProcessingResultDisplaySource DisplaySource() const { return display_source_; }
    std::wstring DisplaySourceLabel() const;
    std::wstring DisplayKindLabel() const;

    void Clear();
    bool Apply(ProcessingJobResult result);
    bool ShowEdfCompositeFrame();
    bool ShowEdfFocusMap();

private:
    ImageFrame processing_result_;
    ImageFrame edf_composite_frame_;
    ImageFrame edf_focus_map_;
    bool show_processing_result_ = false;
    ProcessingResultDisplaySource display_source_ = ProcessingResultDisplaySource::None;
};
