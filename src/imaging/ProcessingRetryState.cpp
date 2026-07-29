#include "ProcessingRetryState.h"

#include "ProcessingParameterRules.h"

#include <utility>

bool ProcessingRetryRequest::CanRetry() const
{
    if (kind == ProcessingJobKind::Stitch) {
        return !stitch_tiles.empty();
    }
    if (kind == ProcessingJobKind::Edf) {
        return edf_stack.size() >= 2;
    }
    return false;
}

ProcessingRetryState::ProcessingRetryState()
{
    Clear();
}

void ProcessingRetryState::Clear()
{
    request_ = ProcessingRetryRequest();
    request_.stitch_search_percent = ProcessingParameterRules::DefaultStitchSearchPercent();
    request_.stitch_use_orb_registration = true;
    request_.stitch_options.overlap_percent = ProcessingParameterRules::DefaultStitchOverlapPercent();
    request_.stitch_options.registration_method = StitchRegistrationMethod::Micro;
    request_.edf_options = ProcessingParameterRules::DefaultEdfOptions();
}

void ProcessingRetryState::RememberStitch(
    std::vector<StitchTile> tiles,
    int search_percent,
    bool use_orb_registration)
{
    Clear();
    request_.kind = ProcessingJobKind::Stitch;
    request_.stitch_tiles = std::move(tiles);
    request_.stitch_search_percent = search_percent;
    request_.stitch_use_orb_registration = use_orb_registration;
    request_.stitch_options.overlap_percent = ProcessingParameterRules::OverlapPercentFromSearch(search_percent);
    request_.stitch_options.registration_method = use_orb_registration ?
        StitchRegistrationMethod::Micro :
        StitchRegistrationMethod::Phase;
}

void ProcessingRetryState::RememberStitch(
    std::vector<StitchTile> tiles,
    StitchProcessingOptions options)
{
    Clear();
    request_.kind = ProcessingJobKind::Stitch;
    request_.stitch_tiles = std::move(tiles);
    request_.stitch_options = options;
    request_.stitch_search_percent = ProcessingParameterRules::SearchPercentFromOverlap(options.overlap_percent);
    request_.stitch_use_orb_registration = options.registration_method != StitchRegistrationMethod::Phase;
}
void ProcessingRetryState::RememberEdf(std::vector<ImageFrame> stack, EdfOptions options)
{
    Clear();
    request_.kind = ProcessingJobKind::Edf;
    request_.edf_stack = std::move(stack);
    request_.edf_options = options;
}

ProcessingRetryRequest ProcessingRetryState::Request() const
{
    return request_;
}
