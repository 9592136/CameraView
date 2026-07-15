#include "ProcessingStatusFormatter.h"

std::wstring ProcessingStatusFormatter::FormatCleared(bool job_running)
{
    return job_running
        ? L"Processing stacks cleared. Running job result will be ignored."
        : L"Processing stacks cleared.";
}

std::wstring ProcessingStatusFormatter::FormatAlreadyRunning()
{
    return L"Processing job is already running.";
}

std::wstring ProcessingStatusFormatter::FormatNoRetry(ProcessingJobKind kind)
{
    switch (kind) {
    case ProcessingJobKind::Stitch:
        return L"No stitch processing job to retry.";
    case ProcessingJobKind::Edf:
        return L"No EDF processing job to retry.";
    case ProcessingJobKind::None:
    default:
        return L"No processing job to retry.";
    }
}

std::wstring ProcessingStatusFormatter::FormatRetryStarted(ProcessingJobKind kind)
{
    switch (kind) {
    case ProcessingJobKind::Stitch:
        return L"Retrying stitch processing in background.";
    case ProcessingJobKind::Edf:
        return L"Retrying EDF processing in background.";
    case ProcessingJobKind::None:
    default:
        return L"No processing job to retry.";
    }
}

std::wstring ProcessingStatusFormatter::FormatStarted(ProcessingJobKind kind)
{
    switch (kind) {
    case ProcessingJobKind::Stitch:
        return L"Stitch processing started in background.";
    case ProcessingJobKind::Edf:
        return L"EDF processing started in background.";
    case ProcessingJobKind::None:
    default:
        return L"Processing started in background.";
    }
}

std::wstring ProcessingStatusFormatter::FormatProgress(ProcessingJobKind kind, int percent)
{
    return KindLabel(kind) + L" processing " + std::to_wstring(percent) + L"%.";
}

std::wstring ProcessingStatusFormatter::FormatCanceled(ProcessingJobKind kind)
{
    return KindLabel(kind) + L" processing canceled.";
}

std::wstring ProcessingStatusFormatter::FormatFailed(ProcessingJobKind kind)
{
    switch (kind) {
    case ProcessingJobKind::Stitch:
        return L"Failed to build stitched image.";
    case ProcessingJobKind::Edf:
        return L"Failed to build EDF image.";
    case ProcessingJobKind::None:
    default:
        return L"Failed to build processed image.";
    }
}

std::wstring ProcessingStatusFormatter::FormatReady(ProcessingJobKind kind, const ImageFrame& image, int relation_count)
{
    switch (kind) {
    case ProcessingJobKind::Stitch:
        return L"Stitched image ready: " + std::to_wstring(image.width) +
               L"x" + std::to_wstring(image.height) +
               L". Optimized " + std::to_wstring(relation_count) + L" relation(s).";
    case ProcessingJobKind::Edf:
        return L"EDF image ready: " + std::to_wstring(image.width) +
               L"x" + std::to_wstring(image.height) + L".";
    case ProcessingJobKind::None:
    default:
        return L"Processed image ready: " + std::to_wstring(image.width) +
               L"x" + std::to_wstring(image.height) + L".";
    }
}

std::wstring ProcessingStatusFormatter::KindLabel(ProcessingJobKind kind)
{
    switch (kind) {
    case ProcessingJobKind::Stitch:
        return L"Stitch";
    case ProcessingJobKind::Edf:
        return L"EDF";
    case ProcessingJobKind::None:
    default:
        return L"Processing";
    }
}
