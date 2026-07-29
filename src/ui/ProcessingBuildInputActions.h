#pragma once

#include "../imaging/ProcessingBuildActions.h"

#include <string>
#include <vector>

struct ProcessingIntegerInputResult {
    bool accepted = true;
    int value = 0;
    std::wstring message;
};

class ProcessingBuildInputActions {
public:
    static ProcessingIntegerInputResult StitchSearchForNextTile(
        bool should_parse,
        const std::wstring& text,
        int current_search_percent);

    static ProcessingIntegerInputResult StitchOverlapForNextTile(
        bool should_parse,
        const std::wstring& text,
        int current_overlap_percent);

    static ProcessingBuildActionResult PrepareStitch(
        const std::vector<StitchTile>& tiles,
        const std::wstring& search_percent_text,
        bool use_orb_registration = true);

    static ProcessingBuildActionResult PrepareStitch(
        const std::vector<StitchTile>& tiles,
        const std::wstring& overlap_percent_text,
        StitchProcessingOptions options);

    static ProcessingBuildActionResult PrepareEdf(
        const std::vector<ImageFrame>& stack,
        const std::wstring& focus_radius_text);
};
