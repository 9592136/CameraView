#include "ProjectSessionMapper.h"

#include "../imaging/DyeLibrary.h"
#include "../imaging/ProcessingParameterRules.h"

#include <utility>

ProjectDocument ProjectSessionMapper::ToDocument(
    const CalibrationProfile& calibration,
    const MeasurementCollection& measurements,
    const std::vector<DyeProfile>& dye_profiles,
    const std::vector<FluorescenceChannel>& fluorescence_channels,
    const EdfOptions& edf_options,
    int stitch_search_percent)
{
    ProjectDocument document;
    document.calibration = calibration;
    document.measurements = measurements.Lengths();
    document.angle_measurements = measurements.Angles();
    document.rectangle_measurements = measurements.Rectangles();
    document.polygon_measurements = measurements.Polygons();
    document.dye_profiles = dye_profiles;
    document.processing_settings.edf_focus_radius = edf_options.focus_radius;
    document.processing_settings.stitch_search_percent = stitch_search_percent;

    document.fluorescence_channels.reserve(fluorescence_channels.size());
    for (const FluorescenceChannel& channel : fluorescence_channels) {
        document.fluorescence_channels.push_back(ToRecipe(channel));
    }

    return document;
}

ProjectSessionState ProjectSessionMapper::FromDocument(ProjectDocument document)
{
    ProjectSessionState state;
    state.calibration = document.calibration;
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
