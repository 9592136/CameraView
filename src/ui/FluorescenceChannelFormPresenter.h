#pragma once

#include "../imaging/Fluorescence.h"

#include <string>

struct FluorescenceChannelFormValues {
    bool visible = false;
    std::wstring black_level;
    std::wstring white_level;
};

class FluorescenceChannelFormPresenter {
public:
    static FluorescenceChannelFormValues Empty();
    static FluorescenceChannelFormValues FromChannel(const FluorescenceChannel& channel);
};
