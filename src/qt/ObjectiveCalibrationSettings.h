#pragma once

#include "domain/CalibrationProfile.h"

#include <vector>

class QSettings;

struct ObjectiveCalibrationState {
    std::vector<std::wstring> labels;
    std::vector<CalibrationProfile> calibrations;
    int selected_index = 0;
};

class ObjectiveCalibrationSettings {
public:
    static ObjectiveCalibrationState Defaults();
    static ObjectiveCalibrationState Load(QSettings& settings);
    static void Save(QSettings& settings, const ObjectiveCalibrationState& state);
};
