#include "ProcessingJobState.h"

#include <utility>

bool ProcessingJobState::IsRunning() const
{
    return running_.load();
}

ProcessingJobLaunch ProcessingJobState::Begin()
{
    const unsigned long long job_id = ++job_counter_;
    active_job_id_ = job_id;
    auto cancel_token = std::make_shared<std::atomic_bool>(false);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cancel_token_ = cancel_token;
        pending_result_ = ProcessingJobResult();
    }
    running_ = true;
    return ProcessingJobLaunch{job_id, std::move(cancel_token)};
}

void ProcessingJobState::Publish(ProcessingJobResult result)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        result.has_result = true;
        pending_result_ = std::move(result);
    }
    running_ = false;
}

ProcessingJobResult ProcessingJobState::TakePending()
{
    std::lock_guard<std::mutex> lock(mutex_);
    ProcessingJobResult result = std::move(pending_result_);
    pending_result_ = ProcessingJobResult();
    return result;
}

void ProcessingJobState::ClearPending()
{
    std::lock_guard<std::mutex> lock(mutex_);
    pending_result_ = ProcessingJobResult();
}

void ProcessingJobState::RequestCancel()
{
    std::shared_ptr<std::atomic_bool> cancel_token;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cancel_token = cancel_token_;
    }
    if (cancel_token) {
        cancel_token->store(true);
    }
}

void ProcessingJobState::InvalidateActiveJob()
{
    active_job_id_ = ++job_counter_;
    ClearPending();
}

void ProcessingJobState::MarkIdle()
{
    running_ = false;
}

bool ProcessingJobState::IsCurrentResult(const ProcessingJobResult& result) const
{
    return result.has_result && result.job_id == active_job_id_.load();
}
