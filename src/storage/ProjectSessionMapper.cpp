#include "ProjectSessionMapper.h"

#include "../imaging/DyeLibrary.h"
#include "../imaging/ProcessingParameterRules.h"

#include <algorithm>
#include <utility>

namespace {

std::vector<std::wstring> DefaultObjectiveLabels()
{
    return CalibrationProfile::ObjectiveMagnificationOptions();
}

int NormalizeObjectiveIndex(int index, std::size_t count)
{
    if (count == 0 || index < 0 || index >= static_cast<int>(count)) {
        return 0;
    }
    return index;
}

int ObjectiveIndexForLabel(
    const std::vector<std::wstring>& labels,
    const std::wstring& label)
{
    const auto match = std::find(labels.begin(), labels.end(), label);
    if (match == labels.end()) {
        return -1;
    }
    return static_cast<int>(std::distance(labels.begin(), match));
}

bool ContainsObjectiveLabel(
    const std::vector<std::wstring>& labels,
    const std::wstring& label)
{
    return ObjectiveIndexForLabel(labels, label) >= 0;
}

} // namespace

ProjectDocument ProjectSessionMapper::ToDocument(
    const CalibrationProfile& calibration,
    const MeasurementCollection& measurements,
    const std::vector<DyeProfile>& dye_profiles,
    const std::vector<FluorescenceChannel>& fluorescence_channels,
    const EdfOptions& edf_options,
    int stitch_search_percent,
    const std::vector<std::wstring>& objective_labels,
    const std::vector<CalibrationProfile>& objective_calibrations,
    int selected_objective_index)
{
    ProjectDocument document;
    const std::vector<std::wstring> objectives = objective_labels.empty()
        ? DefaultObjectiveLabels()
        : objective_labels;
    const int normalized_objective_index =
        NormalizeObjectiveIndex(selected_objective_index, objectives.size());
    document.calibration = calibration;
    document.selected_objective = objectives[static_cast<std::size_t>(normalized_objective_index)];
    document.measurements = measurements.Lengths();
    document.angle_measurements = measurements.Angles();
    document.rectangle_measurements = measurements.Rectangles();
    document.polygon_measurements = measurements.Polygons();
    document.dye_profiles = dye_profiles;
    document.processing_settings.edf_focus_radius = edf_options.focus_radius;
    document.processing_settings.stitch_search_percent = stitch_search_percent;

    const bool has_objective_calibrations = !objective_calibrations.empty();
    document.objective_calibrations.reserve(objectives.size());
    for (std::size_t index = 0; index < objectives.size(); ++index) {
        ObjectiveCalibrationDocument objective_calibration;
        objective_calibration.objective = objectives[index];
        objective_calibration.calibration = index < objective_calibrations.size()
            ? objective_calibrations[index]
            : CalibrationProfile::Uncalibrated();
        if (!has_objective_calibrations &&
            index == static_cast<std::size_t>(normalized_objective_index)) {
            objective_calibration.calibration = calibration;
        }
        document.objective_calibrations.push_back(std::move(objective_calibration));
    }

    document.fluorescence_channels.reserve(fluorescence_channels.size());
    for (const FluorescenceChannel& channel : fluorescence_channels) {
        document.fluorescence_channels.push_back(ToRecipe(channel));
    }

    return document;
}

ProjectSessionState ProjectSessionMapper::FromDocument(ProjectDocument document)
{
    ProjectSessionState state;

    if (document.objective_calibrations.empty()) {
        state.objective_labels = DefaultObjectiveLabels();
        state.selected_objective_index =
            ObjectiveIndexForLabel(state.objective_labels, document.selected_objective);
        if (state.selected_objective_index < 0 && !document.selected_objective.empty()) {
            state.objective_labels.push_back(document.selected_objective);
            state.selected_objective_index = static_cast<int>(state.objective_labels.size() - 1U);
        }
        state.selected_objective_index =
            NormalizeObjectiveIndex(state.selected_objective_index, state.objective_labels.size());
        state.objective_calibrations.assign(
            state.objective_labels.size(),
            CalibrationProfile::Uncalibrated());
        if (document.calibration.IsCalibrated()) {
            state.objective_calibrations[static_cast<std::size_t>(state.selected_objective_index)] =
                document.calibration;
        }
    } else {
        state.objective_labels.reserve(document.objective_calibrations.size());
        state.objective_calibrations.reserve(document.objective_calibrations.size());
        for (const ObjectiveCalibrationDocument& objective_calibration : document.objective_calibrations) {
            if (ContainsObjectiveLabel(state.objective_labels, objective_calibration.objective)) {
                continue;
            }
            state.objective_labels.push_back(objective_calibration.objective);
            state.objective_calibrations.push_back(objective_calibration.calibration);
        }
        if (state.objective_labels.empty()) {
            state.objective_labels = DefaultObjectiveLabels();
            state.objective_calibrations.assign(
                state.objective_labels.size(),
                CalibrationProfile::Uncalibrated());
        }
        state.selected_objective_index =
            ObjectiveIndexForLabel(state.objective_labels, document.selected_objective);
        state.selected_objective_index =
            NormalizeObjectiveIndex(state.selected_objective_index, state.objective_labels.size());
        if (state.objective_calibrations.size() < state.objective_labels.size()) {
            state.objective_calibrations.resize(
                state.objective_labels.size(),
                CalibrationProfile::Uncalibrated());
        }
    }
    state.calibration =
        state.objective_calibrations[static_cast<std::size_t>(state.selected_objective_index)];
    state.measurements.SetAll(
        std::move(document.measurements),
        std::move(document.angle_measurements),
        std::move(document.rectangle_measurements),
        std::move(document.polygon_measurements));
    state.dye_profiles = document.dye_profiles.empty()
        ? DyeLibrary::DefaultDyes()
        : std::move(document.dye_profiles);
    state.edf_options.focus_radius =
        ProcessingParameterRules::ClampEdfFocusRadius(document.processing_settings.edf_focus_radius);
    state.stitch_search_percent =
        ProcessingParameterRules::ClampStitchSearchPercent(document.processing_settings.stitch_search_percent);
    state.restored_channel_settings = !document.fluorescence_channels.empty();

    state.fluorescence_channels.reserve(document.fluorescence_channels.size());
    for (const FluorescenceChannelRecipe& recipe : document.fluorescence_channels) {
        state.fluorescence_channels.push_back(ToChannel(recipe));
    }

    return state;
}

FluorescenceChannelRecipe ProjectSessionMapper::ToRecipe(const FluorescenceChannel& channel)
{
    FluorescenceChannelRecipe recipe;
    recipe.name = channel.name;
    recipe.color = channel.color;
    recipe.visible = channel.visible;
    recipe.black_level = channel.black_level;
    recipe.white_level = channel.white_level;
    return recipe;
}

FluorescenceChannel ProjectSessionMapper::ToChannel(const FluorescenceChannelRecipe& recipe)
{
    FluorescenceChannel channel;
    channel.name = recipe.name;
    channel.color = recipe.color;
    channel.visible = recipe.visible;
    channel.black_level = recipe.black_level;
    channel.white_level = recipe.white_level;
    return channel;
}
