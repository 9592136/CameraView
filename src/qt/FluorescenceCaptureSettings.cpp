#include "FluorescenceCaptureSettings.h"

#include <QSettings>
#include <QString>

#include <algorithm>
#include <cmath>

namespace {

constexpr auto kSettingsGroup = "FluorescenceCapturePresets";
constexpr auto kPresetsArray = "presets";

bool IsValidColor(const RgbColor& color)
{
    return color.r != 0 || color.g != 0 || color.b != 0;
}

} // namespace

std::vector<FluorescenceCapturePreset> FluorescenceCaptureSettings::Defaults(
    const std::vector<DyeProfile>& dyes)
{
    std::vector<FluorescenceCapturePreset> presets;
    presets.reserve(dyes.size());
    for (const DyeProfile& dye : dyes) {
        presets.push_back({dye.name, 10.0, dye.color});
    }
    return presets;
}

std::vector<FluorescenceCapturePreset> FluorescenceCaptureSettings::Load(
    QSettings& settings,
    const std::vector<DyeProfile>& dyes)
{
    settings.beginGroup(QString::fromLatin1(kSettingsGroup));
    const bool initialized = settings.value(QStringLiteral("initialized"), false).toBool();
    const int count = settings.beginReadArray(QString::fromLatin1(kPresetsArray));
    std::vector<FluorescenceCapturePreset> presets;
    presets.reserve(static_cast<std::size_t>(std::max(0, count)));
    for (int index = 0; index < count; ++index) {
        settings.setArrayIndex(index);
        FluorescenceCapturePreset preset;
        preset.dye_name = settings.value(QStringLiteral("dyeName")).toString().trimmed().toStdWString();
        preset.exposure_ms = settings.value(QStringLiteral("exposureMs"), 10.0).toDouble();
        preset.color = {
            static_cast<unsigned char>(std::clamp(settings.value(QStringLiteral("red"), 255).toInt(), 0, 255)),
            static_cast<unsigned char>(std::clamp(settings.value(QStringLiteral("green"), 255).toInt(), 0, 255)),
            static_cast<unsigned char>(std::clamp(settings.value(QStringLiteral("blue"), 255).toInt(), 0, 255))};
        if (!preset.dye_name.empty() && std::isfinite(preset.exposure_ms) &&
            preset.exposure_ms > 0.0 && IsValidColor(preset.color)) {
            const auto duplicate = std::find_if(presets.begin(), presets.end(), [&preset](const auto& existing) {
                return existing.dye_name == preset.dye_name;
            });
            if (duplicate == presets.end()) presets.push_back(preset);
        }
    }
    settings.endArray();
    settings.endGroup();
    return !initialized ? Defaults(dyes) : presets;
}

void FluorescenceCaptureSettings::Save(
    QSettings& settings,
    const std::vector<FluorescenceCapturePreset>& presets)
{
    settings.beginGroup(QString::fromLatin1(kSettingsGroup));
    settings.setValue(QStringLiteral("initialized"), true);
    settings.beginWriteArray(QString::fromLatin1(kPresetsArray), static_cast<int>(presets.size()));
    for (std::size_t index = 0; index < presets.size(); ++index) {
        const FluorescenceCapturePreset& preset = presets[index];
        settings.setArrayIndex(static_cast<int>(index));
        settings.setValue(QStringLiteral("dyeName"), QString::fromStdWString(preset.dye_name));
        settings.setValue(QStringLiteral("exposureMs"), preset.exposure_ms);
        settings.setValue(QStringLiteral("red"), preset.color.r);
        settings.setValue(QStringLiteral("green"), preset.color.g);
        settings.setValue(QStringLiteral("blue"), preset.color.b);
    }
    settings.endArray();
    settings.endGroup();
    settings.sync();
}
