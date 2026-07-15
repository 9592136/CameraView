#pragma once

#include "../domain/ImageFrame.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

enum class ProcessingJobKind {
    None,
    Stitch,
    Edf
};

struct ProcessingJobResult {
    unsigned long long job_id = 0;
    ProcessingJobKind kind = ProcessingJobKind::None;
    bool has_result = false;
    bool succeeded = false;
    ImageFrame image;
    ImageFrame focus_map;
    std::wstring status;
};

struct ProcessingJobLaunch {
    unsigned long long job_id = 0;
    std::shared_ptr<std::atomic_bool> cancel_token;
};

class ProcessingJobState {
public:
    bool IsRunning() const;
    ProcessingJobLaunch Begin();
    void Publish(ProcessingJobResult result);
    ProcessingJobResult TakePending();
    void ClearPending();
    void RequestCancel();
    void InvalidateActiveJob();
    void MarkIdle();
    bool IsCurrentResult(const ProcessingJobResult& result) const;

private:
    std::atomic_bool running_ = false;
    std::atomic_ullong job_counter_ = 0;
    std::atomic_ullong active_job_id_ = 0;
    std::shared_ptr<std::atomic_bool> cancel_token_;
    mutable std::mutex mutex_;
    ProcessingJobResult pending_result_;
};
