#include "ProcessingResultFrames.h"

#include <utility>

void ProcessingResultFrames::Clear()
{
    processing_result_ = ImageFrame();
    edf_composite_frame_ = ImageFrame();
    edf_focus_map_ = ImageFrame();
    show_processing_result_ = false;
    display_source_ = ProcessingResultDisplaySource::None;
}

bool ProcessingResultFrames::Apply(ProcessingJobResult result)
{
    if (!result.succeeded) {
        return false;
    }

    if (result.kind == ProcessingJobKind::Edf) {
        edf_composite_frame_ = result.image;
        edf_focus_map_ = std::move(result.focus_map);
        processing_result_ = std::move(result.image);
        display_source_ = ProcessingResultDisplaySource::EdfComposite;
    } else {
        edf_composite_frame_ = ImageFrame();
        edf_focus_map_ = ImageFrame();
        processing_result_ = std::move(result.image);
        display_source_ = ProcessingResultDisplaySource::Stitch;
    }

    show_processing_result_ = processing_result_.IsValid();
    if (!show_processing_result_) {
        display_source_ = ProcessingResultDisplaySource::None;
    }
    return show_processing_result_;
}

bool ProcessingResultFrames::ShowEdfCompositeFrame()
{
    if (!edf_composite_frame_.IsValid()) {
        return false;
    }

    processing_result_ = edf_composite_frame_;
    show_processing_result_ = true;
    display_source_ = ProcessingResultDisplaySource::EdfComposite;
    return true;
}

bool ProcessingResultFrames::ShowEdfFocusMap()
{
    if (!edf_focus_map_.IsValid()) {
        return false;
    }

    processing_result_ = edf_focus_map_;
    show_processing_result_ = true;
    display_source_ = ProcessingResultDisplaySource::EdfFocusMap;
    return true;
}

std::wstring ProcessingResultFrames::DisplaySourceLabel() const
{
    switch (display_source_) {
    case ProcessingResultDisplaySource::Stitch:
        return L"Stitch result";
    case ProcessingResultDisplaySource::EdfComposite:
        return L"EDF composite";
    case ProcessingResultDisplaySource::EdfFocusMap:
        return L"EDF focus map";
    case ProcessingResultDisplaySource::None:
    default:
        return L"(none)";
    }
}

std::wstring ProcessingResultFrames::DisplayKindLabel() const
{
    switch (display_source_) {
    case ProcessingResultDisplaySource::Stitch:
        return L"Stitch";
    case ProcessingResultDisplaySource::EdfComposite:
    case ProcessingResultDisplaySource::EdfFocusMap:
        return L"EDF";
    case ProcessingResultDisplaySource::None:
    default:
        return L"";
    }
}
