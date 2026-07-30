#pragma once

#include "ProcessingJobState.h"
#include "i18n/Localization.h"

#include <string>

class ProcessingStatusFormatter {
public:
    static std::wstring FormatCleared(bool job_running, UILanguage lang = UILanguage::English);
    static std::wstring FormatAlreadyRunning(UILanguage lang = UILanguage::English);
    static std::wstring FormatNoRetry(ProcessingJobKind kind, UILanguage lang = UILanguage::English);
    static std::wstring FormatRetryStarted(ProcessingJobKind kind, UILanguage lang = UILanguage::English);
    static std::wstring FormatStarted(ProcessingJobKind kind, UILanguage lang = UILanguage::English);
    static std::wstring FormatProgress(ProcessingJobKind kind, int percent, UILanguage lang = UILanguage::English);
    static std::wstring FormatCanceled(ProcessingJobKind kind, UILanguage lang = UILanguage::English);
    static std::wstring FormatFailed(ProcessingJobKind kind, UILanguage lang = UILanguage::English);
    static std::wstring FormatReady(ProcessingJobKind kind, const ImageFrame& image,
                                    int relation_count = 0, UILanguage lang = UILanguage::English);

private:
    static std::wstring KindLabel(ProcessingJobKind kind, UILanguage lang = UILanguage::English);
};
