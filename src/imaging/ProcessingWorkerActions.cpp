#include "ProcessingWorkerActions.h"

#include "ProcessingJobExecutor.h"
#include "ProcessingProgressActions.h"

#include <atomic>
#include <memory>
#include <utility>

std::thread ProcessingWorkerActions::StartStitch(
    ProcessingJobLaunch launch,
    std::vector<StitchTile> tiles,
    int search_percent,
    StatusCallback status_callback,
    PublishCallback publish_callback)
{
    const unsigned long long job_id = launch.job_id;
    std::shared_ptr<std::atomic_bool> cancel_token = launch.cancel_token;
    return std::thread(
        [job_id,
         cancel_token,
         search_percent,
         tiles = std::move(tiles),
         status_callback = std::move(status_callback),
         publish_callback = std::move(publish_callback)]() mutable {
            auto progress = [status_callback, reporter = ProcessingProgressActions(ProcessingJobKind::Stitch, cancel_token)](
                                int percent) mutable {
                const ProcessingProgressActionResult progress_result = reporter.Report(percent);
                if (progress_result.should_report && status_callback) {
                    status_callback(progress_result.message);
                }
            };
            ProcessingJobResult result = ProcessingJobExecutor::RunStitch(
                job_id,
                std::move(tiles),
                search_percent,
                cancel_token.get(),
                progress);
            if (publish_callback) {
                publish_callback(std::move(result));
            }
        });
}

std::thread ProcessingWorkerActions::StartEdf(
    ProcessingJobLaunch launch,
    std::vector<ImageFrame> stack,
    EdfOptions options,
    StatusCallback status_callback,
    PublishCallback publish_callback)
{
    const unsigned long long job_id = launch.job_id;
    std::shared_ptr<std::atomic_bool> cancel_token = launch.cancel_token;
    return std::thread(
        [job_id,
         cancel_token,
         options,
         stack = std::move(stack),
         status_callback = std::move(status_callback),
         publish_callback = std::move(publish_callback)]() mutable {
            auto progress = [status_callback, reporter = ProcessingProgressActions(ProcessingJobKind::Edf, cancel_token)](
                                int percent) mutable {
                const ProcessingProgressActionResult progress_result = reporter.Report(percent);
                if (progress_result.should_report && status_callback) {
                    status_callback(progress_result.message);
                }
            };
            ProcessingJobResult result = ProcessingJobExecutor::RunEdf(
                job_id,
                std::move(stack),
                options,
                cancel_token.get(),
                progress);
            if (publish_callback) {
                publish_callback(std::move(result));
            }
        });
}
