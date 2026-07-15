#pragma once

#include "ProcessingJobState.h"

#include <string>

class ProcessingStatusFormatter {
public:
    static std::wstring FormatCleared(bool job_running);
    static std::wstring FormatAlreadyRunning();
    static std::wstring FormatNoRetry(ProcessingJobKind kind);
    static std::wstring FormatRetryStarted(ProcessingJobKind kind);
    static std::wstring FormatStarted(ProcessingJobKind kind);
    static std::wstring FormatProgress(ProcessingJobKind kind, int percent);
    static std::wstring FormatCanceled(ProcessingJobKind kind);
    static std::wstring FormatFailed(ProcessingJobKind kind);
    static std::wstring FormatReady(ProcessingJobKind kind, const ImageFrame& image, int relation_count = 0);

private:
    static std::wstring KindLabel(ProcessingJobKind kind);
};
