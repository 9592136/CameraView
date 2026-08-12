#include "ObjectiveCalibrationSettings.h"

#include <QSettings>
#include <QString>

#include <algorithm>

namespace {

constexpr auto kSettingsGroup = "ObjectiveCalibrations";
constexpr auto kProfilesArray = "profiles";
constexpr auto kSchemaVersion = "schemaVersion";

int IndexForLabel(const std::vector<std::wstring>& labels, const std::wstring& label)
{
    const auto match = std::find(labels.begin(), labels.end(), label);
    return match == labels.end()
        ? -1
        : static_cast<int>(std::distance(labels.begin(), match));
}

int NormalizeIndex(int index, std::size_t size)
{
    return size > 0 && index >= 0 && index < static_cast<int>(size) ? index : 0;
}

void AppendObjective(
    ObjectiveCalibrationState& state,
    const std::wstring& label,
    const CalibrationProfile& calibration)
{
    if (label.empty() || IndexForLabel(state.labels, label) >= 0) {
        return;
    }
    state.labels.push_back(label);
    state.calibrations.push_back(calibration);
}

} // namespace

ObjectiveCalibrationState ObjectiveCalibrationSettings::Defaults()
{
    ObjectiveCalibrationState state;
    state.labels = CalibrationProfile::ObjectiveMagnificationOptions();
    state.calibrations.assign(
        state.labels.size(), CalibrationProfile::Uncalibrated());
    return state;
}

ObjectiveCalibrationState ObjectiveCalibrationSettings::Load(QSettings& settings)
{
    ObjectiveCalibrationState state;
    settings.beginGroup(QString::fromLatin1(kSettingsGroup));
    const bool has_saved_collection = settings.value(
        QString::fromLatin1(kSchemaVersion), 0).toInt() >= 2;
    const QString selected_label = settings.value(QStringLiteral("selectedObjective")).toString();
    const int profile_count = settings.beginReadArray(QString::fromLatin1(kProfilesArray));
    for (int index = 0; index < profile_count; ++index) {
        settings.setArrayIndex(index);
        const std::wstring label = settings.value(QStringLiteral("label")).toString().toStdWString();
        const double microns_per_pixel = settings.value(
            QStringLiteral("micronsPerPixel"), 0.0).toDouble();
        AppendObjective(
            state,
            label,
            CalibrationProfile::FromMicronsPerPixel(microns_per_pixel));
    }
    settings.endArray();
    settings.endGroup();

    if (!has_saved_collection) {
        for (const std::wstring& label : CalibrationProfile::ObjectiveMagnificationOptions()) {
            AppendObjective(state, label, CalibrationProfile::Uncalibrated());
        }
    }
    if (state.labels.empty()) {
        state = Defaults();
    }
    if (state.calibrations.size() < state.labels.size()) {
        state.calibrations.resize(
            state.labels.size(), CalibrationProfile::Uncalibrated());
    }
    state.selected_index = NormalizeIndex(
        IndexForLabel(state.labels, selected_label.toStdWString()), state.labels.size());
    return state;
}

void ObjectiveCalibrationSettings::Save(
    QSettings& settings,
    const ObjectiveCalibrationState& state)
{
    if (state.labels.empty()) {
        return;
    }
    const int selected_index = NormalizeIndex(state.selected_index, state.labels.size());
    settings.beginGroup(QString::fromLatin1(kSettingsGroup));
    settings.setValue(QString::fromLatin1(kSchemaVersion), 2);
    settings.setValue(
        QStringLiteral("selectedObjective"),
        QString::fromStdWString(state.labels[static_cast<std::size_t>(selected_index)]));
    settings.beginWriteArray(
        QString::fromLatin1(kProfilesArray), static_cast<int>(state.labels.size()));
    for (std::size_t index = 0; index < state.labels.size(); ++index) {
        settings.setArrayIndex(static_cast<int>(index));
        settings.setValue(
            QStringLiteral("label"), QString::fromStdWString(state.labels[index]));
        const CalibrationProfile calibration = index < state.calibrations.size()
            ? state.calibrations[index]
            : CalibrationProfile::Uncalibrated();
        settings.setValue(
            QStringLiteral("micronsPerPixel"), calibration.MicronsPerPixel());
    }
    settings.endArray();
    settings.endGroup();
    settings.sync();
}
