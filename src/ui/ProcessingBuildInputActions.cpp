#include "ProcessingBuildInputActions.h"

#include "../imaging/ProcessingParameterRules.h"
#include "../platform/TextInputParser.h"

ProcessingIntegerInputResult ProcessingBuildInputActions::StitchSearchForNextTile(
    bool should_parse,
    const std::wstring& text,
    int current_search_percent)
{
    ProcessingIntegerInputResult result;
    result.value = current_search_percent;
    if (!should_parse) {
        return result;
    }

    if (!TextInputParser::TryParseIntegerRange(
            text,
            ProcessingParameterRules::MinStitchSearchPercent(),
            ProcessingParameterRules::MaxStitchSearchPercent(),
            result.value)) {
        result.accepted = false;
        result.message = L"Stitch search must be 5-100 percent.";
    }
    return result;
}

ProcessingIntegerInputResult ProcessingBuildInputActions::StitchOverlapForNextTile(
    bool should_parse,
    const std::wstring& text,
    int current_overlap_percent)
{
    ProcessingIntegerInputResult result;
    result.value = current_overlap_percent;
    if (!should_parse) {
        return result;
    }

    if (!TextInputParser::TryParseIntegerRange(
            text,
            ProcessingParameterRules::MinStitchOverlapPercent(),
            ProcessingParameterRules::MaxStitchOverlapPercent(),
            result.value)) {
        result.accepted = false;
        result.message = L"Estimated overlap must be 5-50 percent.";
    }
    return result;
}
ProcessingBuildActionResult ProcessingBuildInputActions::PrepareStitch(
    const std::vector<StitchTile>& tiles,
    const std::wstring& search_percent_text,
    bool use_orb_registration)
{
    int search_percent = ProcessingParameterRules::DefaultStitchSearchPercent();
    const bool search_percent_valid = TextInputParser::TryParseIntegerRange(
        search_percent_text,
        ProcessingParameterRules::MinStitchSearchPercent(),
        ProcessingParameterRules::MaxStitchSearchPercent(),
        search_percent);
    return ProcessingBuildActions::PrepareStitch(
        tiles,
        search_percent_valid,
        search_percent,
        use_orb_registration);
}

ProcessingBuildActionResult ProcessingBuildInputActions::PrepareStitch(
    const std::vector<StitchTile>& tiles,
    const std::wstring& overlap_percent_text,
    StitchProcessingOptions options)
{
    int overlap_percent = ProcessingParameterRules::DefaultStitchOverlapPercent();
    const bool overlap_percent_valid = TextInputParser::TryParseIntegerRange(
        overlap_percent_text,
        ProcessingParameterRules::MinStitchOverlapPercent(),
        ProcessingParameterRules::MaxStitchOverlapPercent(),
        overlap_percent);
    options.overlap_percent = overlap_percent;
    return ProcessingBuildActions::PrepareStitch(
        tiles,
        overlap_percent_valid,
        options);
}
ProcessingBuildActionResult ProcessingBuildInputActions::PrepareEdf(
    const std::vector<ImageFrame>& stack,
    const std::wstring& focus_radius_text)
{
    int focus_radius = ProcessingParameterRules::DefaultEdfOptions().focus_radius;
    const bool focus_radius_valid = TextInputParser::TryParseIntegerRange(
        focus_radius_text,
        ProcessingParameterRules::MinEdfFocusRadius(),
        ProcessingParameterRules::MaxEdfFocusRadius(),
        focus_radius);
    return ProcessingBuildActions::PrepareEdf(stack, focus_radius_valid, focus_radius);
}
