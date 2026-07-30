#include "ProcessingStatusFormatter.h"
#include "i18n/Localization.h"

std::wstring ProcessingStatusFormatter::FormatCleared(bool job_running, UILanguage lang)
{
    return GetLocStr(job_running ? LocId::PROC_CLEARED_RUNNING : LocId::PROC_CLEARED, lang);
}

std::wstring ProcessingStatusFormatter::FormatAlreadyRunning(UILanguage lang)
{
    return GetLocStr(LocId::PROC_ALREADY_RUNNING, lang);
}

std::wstring ProcessingStatusFormatter::FormatNoRetry(ProcessingJobKind kind, UILanguage lang)
{
    switch (kind) {
    case ProcessingJobKind::Stitch:
        return GetLocStr(LocId::PROC_NO_RETRY_STITCH, lang);
    case ProcessingJobKind::Edf:
        return GetLocStr(LocId::PROC_NO_RETRY_EDF, lang);
    default:
        return GetLocStr(LocId::PROC_NO_RETRY, lang);
    }
}

std::wstring ProcessingStatusFormatter::FormatRetryStarted(ProcessingJobKind kind, UILanguage lang)
{
    switch (kind) {
    case ProcessingJobKind::Stitch:
        return GetLocStr(LocId::PROC_RETRY_STARTED_STITCH, lang);
    case ProcessingJobKind::Edf:
        return GetLocStr(LocId::PROC_RETRY_STARTED_EDF, lang);
    default:
        return GetLocStr(LocId::PROC_RETRY_STARTED, lang);
    }
}

std::wstring ProcessingStatusFormatter::FormatStarted(ProcessingJobKind kind, UILanguage lang)
{
    switch (kind) {
    case ProcessingJobKind::Stitch:
        return GetLocStr(LocId::PROC_STARTED_STITCH, lang);
    case ProcessingJobKind::Edf:
        return GetLocStr(LocId::PROC_STARTED_EDF, lang);
    default:
        return GetLocStr(LocId::PROC_STARTED, lang);
    }
}

std::wstring ProcessingStatusFormatter::FormatProgress(ProcessingJobKind kind, int percent, UILanguage lang)
{
    return FormatLocStr(LocId::PROC_PROGRESS, lang, {
        {L"{kind}", KindLabel(kind, lang)},
        {L"{percent}", std::to_wstring(percent)}
    });
}

std::wstring ProcessingStatusFormatter::FormatCanceled(ProcessingJobKind kind, UILanguage lang)
{
    return FormatLocStr(LocId::PROC_CANCELED, lang, {
        {L"{kind}", KindLabel(kind, lang)}
    });
}

std::wstring ProcessingStatusFormatter::FormatFailed(ProcessingJobKind kind, UILanguage lang)
{
    switch (kind) {
    case ProcessingJobKind::Stitch:
        return GetLocStr(LocId::PROC_FAILED_STITCH, lang);
    case ProcessingJobKind::Edf:
        return GetLocStr(LocId::PROC_FAILED_EDF, lang);
    default:
        return GetLocStr(LocId::PROC_FAILED_GENERIC, lang);
    }
}

std::wstring ProcessingStatusFormatter::FormatReady(ProcessingJobKind kind, const ImageFrame& image, int relation_count, UILanguage lang)
{
    switch (kind) {
    case ProcessingJobKind::Stitch:
        return FormatLocStr(LocId::PROC_READY_STITCH, lang, {
            {L"{w}", std::to_wstring(image.width)},
            {L"{h}", std::to_wstring(image.height)},
            {L"{n}", std::to_wstring(relation_count)}
        });
    case ProcessingJobKind::Edf:
        return FormatLocStr(LocId::PROC_READY_EDF, lang, {
            {L"{w}", std::to_wstring(image.width)},
            {L"{h}", std::to_wstring(image.height)}
        });
    default:
        return FormatLocStr(LocId::PROC_READY_GENERIC, lang, {
            {L"{w}", std::to_wstring(image.width)},
            {L"{h}", std::to_wstring(image.height)}
        });
    }
}

std::wstring ProcessingStatusFormatter::KindLabel(ProcessingJobKind kind, UILanguage lang)
{
    switch (kind) {
    case ProcessingJobKind::Stitch:
        return GetLocStr(LocId::PROC_KIND_STITCH, lang);
    case ProcessingJobKind::Edf:
        return GetLocStr(LocId::PROC_KIND_EDF, lang);
    default:
        return GetLocStr(LocId::PROC_KIND_GENERIC, lang);
    }
}
