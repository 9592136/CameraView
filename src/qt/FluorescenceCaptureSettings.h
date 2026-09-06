#pragma once

#include "imaging/Fluorescence.h"

#include <vector>

class QSettings;

class FluorescenceCaptureSettings {
public:
    static std::vector<FluorescenceCapturePreset> Defaults(
        const std::vector<DyeProfile>& dyes);
    static std::vector<FluorescenceCapturePreset> Load(
        QSettings& settings,
        const std::vector<DyeProfile>& dyes);
    static void Save(
        QSettings& settings,
        const std::vector<FluorescenceCapturePreset>& presets);
};
