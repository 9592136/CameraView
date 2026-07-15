#include "WindowControlLayout.h"

#include "ControlIds.h"
#include "WindowLayout.h"

#include <algorithm>

namespace {

constexpr int kControlGap = 8;
constexpr int kToolbarTop = 10;
constexpr int kControlHeight = 28;
constexpr int kPanelMargin = 12;

void Add(
    std::vector<WindowControlPlacement>& placements,
    int control_id,
    int x,
    int y,
    int width,
    int height,
    bool visible = true)
{
    placements.push_back(WindowControlPlacement{
        control_id,
        RECT{x, y, x + width, y + height},
        visible
    });
}

void AddToolbar(std::vector<WindowControlPlacement>& placements)
{
    int left = 10;

    Add(placements, kIdDeviceLabel, left, kToolbarTop + 5, 55, 20);
    left += 55 + kControlGap;
    Add(placements, kIdDeviceCombo, left, kToolbarTop, 220, 160);
    left += 220 + kControlGap;
    Add(placements, kIdRefreshDevices, left, kToolbarTop, 80, kControlHeight);
    left += 80 + kControlGap + 8;
    Add(placements, kIdOpen, left, kToolbarTop, 80, kControlHeight);
    left += 80 + kControlGap;
    Add(placements, kIdStop, left, kToolbarTop, 80, kControlHeight);
    left += 80 + kControlGap;
    Add(placements, kIdFitView, left, kToolbarTop, 56, kControlHeight);
    left += 56 + kControlGap + 8;
    Add(placements, kIdExposureLabel, left, kToolbarTop + 5, 90, 20);
    left += 90 + kControlGap;
    Add(placements, kIdExposureEdit, left, kToolbarTop, 80, kControlHeight);
    left += 80 + kControlGap;
    Add(placements, kIdExposureApply, left, kToolbarTop, 80, kControlHeight);
}

void AddHiddenSideControls(std::vector<WindowControlPlacement>& placements)
{
    for (int control_id : WindowControlLayout::SideControlIds()) {
        Add(placements, control_id, 0, 0, 0, 0, false);
    }
}

bool ContainsControl(const std::vector<int>& control_ids, int control_id)
{
    return std::find(control_ids.begin(), control_ids.end(), control_id) != control_ids.end();
}

void AddHiddenSideControlsExcept(
    std::vector<WindowControlPlacement>& placements,
    const std::vector<int>& visible_control_ids)
{
    for (int control_id : WindowControlLayout::SideControlIds()) {
        if (!ContainsControl(visible_control_ids, control_id)) {
            Add(placements, control_id, 0, 0, 0, 0, false);
        }
    }
}

void AddSidePanel(std::vector<WindowControlPlacement>& placements, const RECT& panel, int panel_category)
{
    const int x = static_cast<int>(panel.left) + kPanelMargin;
    const int w = std::max(80, static_cast<int>(panel.right - panel.left) - kPanelMargin * 2);
    int y = static_cast<int>(panel.top) + kPanelMargin;
    const int half_width = std::max(40, (w - kControlGap) / 2);
    const int second_column = x + half_width + kControlGap;
    std::vector<int> visible_control_ids;
    auto add = [&](int control_id, int left, int top, int width, int height) {
        Add(placements, control_id, left, top, width, height);
        visible_control_ids.push_back(control_id);
    };

    add(kIdCameraPanelCard, x, y, half_width, kControlHeight);
    add(kIdImagePanelCard, second_column, y, half_width, kControlHeight);
    y += kControlHeight + kControlGap;
    add(kIdFluorescencePanelCard, x, y, half_width, kControlHeight);
    add(kIdProcessingPanelCard, second_column, y, half_width, kControlHeight);
    y += kControlHeight + kControlGap;
    add(kIdMeasurementPanelCard, x, y, half_width, kControlHeight);
    add(kIdProjectPanelCard, second_column, y, half_width, kControlHeight);
    y += kControlHeight + kControlGap + 8;

    switch (WindowControlLayout::NormalizePanelCategory(panel_category)) {
    case 1:
        add(kIdPseudoColorLabel, x, y, w, 20);
        y += 24;
        add(kIdPseudoColorCombo, x, y, w, 140);
        y += kControlHeight + kControlGap;
        add(kIdOpenImage, x, y, half_width, kControlHeight);
        add(kIdExportImage, second_column, y, half_width, kControlHeight);
        break;
    case 2:
        add(kIdDyeLabel, x, y, w, 20);
        y += 24;
        add(kIdDyeCombo, x, y, w, 140);
        y += kControlHeight + kControlGap;
        add(kIdDyeNameLabel, x, y + 5, 42, 20);
        add(kIdDyeNameEdit, x + 48, y, w - 48, kControlHeight);
        y += kControlHeight + kControlGap;
        add(kIdDyeExcitationLabel, x, y + 5, 22, 20);
        add(kIdDyeExcitationEdit, x + 26, y, 54, kControlHeight);
        add(kIdDyeEmissionLabel, x + 88, y + 5, 24, 20);
        add(kIdDyeEmissionEdit, x + 116, y, 54, kControlHeight);
        y += kControlHeight + kControlGap;
        add(kIdDyeRedLabel, x, y + 5, 16, 20);
        add(kIdDyeRedEdit, x + 20, y, 38, kControlHeight);
        add(kIdDyeGreenLabel, x + 64, y + 5, 18, 20);
        add(kIdDyeGreenEdit, x + 84, y, 38, kControlHeight);
        add(kIdDyeBlueLabel, x + 128, y + 5, 18, 20);
        add(kIdDyeBlueEdit, x + 148, y, 38, kControlHeight);
        y += kControlHeight + kControlGap;
        add(kIdSaveDye, x, y, half_width, kControlHeight);
        add(kIdDeleteDye, second_column, y, half_width, kControlHeight);
        y += kControlHeight + kControlGap;
        add(kIdAddChannel, x, y, half_width, kControlHeight);
        add(kIdFusionPreview, second_column, y, half_width, kControlHeight);
        y += kControlHeight + kControlGap;
        add(kIdClearChannels, x, y, w, kControlHeight);
        y += kControlHeight + kControlGap;
        add(kIdChannelLabel, x, y, w, 20);
        y += 24;
        add(kIdChannelList, x, y, w, 96);
        y += 96 + kControlGap;
        add(kIdChannelVisible, x, y, half_width, kControlHeight);
        add(kIdApplyChannel, second_column, y, half_width, kControlHeight);
        y += kControlHeight + kControlGap;
        add(kIdChannelBlackLabel, x, y + 5, 34, 20);
        add(kIdChannelBlackEdit, x + 40, y, 48, kControlHeight);
        add(kIdChannelWhiteLabel, x + 96, y + 5, 38, 20);
        add(kIdChannelWhiteEdit, x + 140, y, 48, kControlHeight);
        break;
    case 3:
        add(kIdStitchSearchLabel, x, y + 5, 98, 20);
        add(kIdStitchSearchEdit, x + 106, y, 54, kControlHeight);
        y += kControlHeight + kControlGap;
        add(kIdAddStitchTile, x, y, half_width, kControlHeight);
        add(kIdBuildStitch, second_column, y, half_width, kControlHeight);
        y += kControlHeight + kControlGap + 8;
        add(kIdEdfRadiusLabel, x, y + 5, 76, 20);
        add(kIdEdfRadiusEdit, x + 84, y, 54, kControlHeight);
        y += kControlHeight + kControlGap;
        add(kIdAddEdfFrame, x, y, half_width, kControlHeight);
        add(kIdBuildEdf, second_column, y, half_width, kControlHeight);
        y += kControlHeight + kControlGap;
        add(kIdShowEdfComposite, x, y, half_width, kControlHeight);
        add(kIdShowEdfFocusMap, second_column, y, half_width, kControlHeight);
        y += kControlHeight + kControlGap;
        add(kIdRetryProcessing, x, y, half_width, kControlHeight);
        add(kIdClearProcessing, second_column, y, half_width, kControlHeight);
        break;
    case 4:
        add(kIdCalibrationLengthLabel, x, y, w, 20);
        y += 24;
        add(kIdCalibrationLengthEdit, x, y, 90, kControlHeight);
        add(kIdCalibrationUnitCombo, x + 98, y, 92, 100);
        y += kControlHeight + kControlGap;
        add(kIdCalibrate, x, y, half_width, kControlHeight);
        add(kIdLengthTool, second_column, y, half_width, kControlHeight);
        y += kControlHeight + kControlGap;
        add(kIdAngleTool, x, y, half_width, kControlHeight);
        add(kIdRectangleAreaTool, second_column, y, half_width, kControlHeight);
        y += kControlHeight + kControlGap;
        add(kIdPolygonAreaTool, x, y, half_width, kControlHeight);
        add(kIdFinishPolygonArea, second_column, y, half_width, kControlHeight);
        y += kControlHeight + kControlGap;
        add(kIdDeleteMeasurement, x, y, half_width, kControlHeight);
        add(kIdClearMeasurements, second_column, y, half_width, kControlHeight);
        y += kControlHeight + kControlGap;
        add(kIdExportCsv, x, y, half_width, kControlHeight);
        add(kIdMeasurementNameEdit, second_column, y, half_width - 64, kControlHeight);
        add(kIdRenameMeasurement, second_column + half_width - 56, y, 56, kControlHeight);
        y += kControlHeight + kControlGap + 8;
        add(kIdResultsLabel, x, y, w, 20);
        y += 24;
        add(kIdResultsList, x, y, w, std::max(0, static_cast<int>(panel.bottom) - y - kPanelMargin));
        break;
    case 5:
        add(kIdOpenProject, x, y, half_width, kControlHeight);
        add(kIdSaveProject, second_column, y, half_width, kControlHeight);
        y += kControlHeight + kControlGap;
        add(kIdSaveDiagnostics, x, y, w, kControlHeight);
        break;
    case 0:
    default:
        add(kIdAutoExposure, x, y, w, kControlHeight);
        y += kControlHeight + kControlGap;
        add(kIdCameraExposureLabel, x, y + 5, 74, 20);
        add(kIdCameraExposureEdit, x + 82, y, 92, kControlHeight);
        add(kIdCameraExposureApply, second_column, y, half_width, kControlHeight);
        y += kControlHeight + kControlGap;
        add(kIdCameraGainLabel, x, y + 5, 74, 20);
        add(kIdCameraGainEdit, x + 82, y, 92, kControlHeight);
        add(kIdCameraGainApply, second_column, y, half_width, kControlHeight);
        y += kControlHeight + kControlGap;
        add(kIdWhiteBalance, x, y, w, kControlHeight);
        break;
    }

    AddHiddenSideControlsExcept(placements, visible_control_ids);
}

} // namespace

std::vector<WindowControlPlacement> WindowControlLayout::Compute(const RECT& client_rect, int panel_category)
{
    std::vector<WindowControlPlacement> placements;
    placements.reserve(9 + SideControlIds().size());
    AddToolbar(placements);

    const RECT panel = WindowLayout::SidePanelRect(client_rect);
    if (panel.right <= panel.left) {
        AddHiddenSideControls(placements);
        return placements;
    }

    AddSidePanel(placements, panel, panel_category);
    return placements;
}

const std::vector<std::wstring>& WindowControlLayout::PanelCategoryLabels()
{
    static const std::vector<std::wstring> labels = {
        L"Camera",
        L"Image",
        L"Fluorescence",
        L"Processing",
        L"Measurement",
        L"Project"
    };
    return labels;
}

int WindowControlLayout::NormalizePanelCategory(int panel_category)
{
    const std::vector<std::wstring>& labels = PanelCategoryLabels();
    if (panel_category < 0 || static_cast<std::size_t>(panel_category) >= labels.size()) {
        return 0;
    }
    return panel_category;
}

int WindowControlLayout::PanelCategoryFromCardControl(int control_id)
{
    switch (control_id) {
    case kIdCameraPanelCard:
        return 0;
    case kIdImagePanelCard:
        return 1;
    case kIdFluorescencePanelCard:
        return 2;
    case kIdProcessingPanelCard:
        return 3;
    case kIdMeasurementPanelCard:
        return 4;
    case kIdProjectPanelCard:
        return 5;
    default:
        return -1;
    }
}

const std::vector<int>& WindowControlLayout::SideControlIds()
{
    static const std::vector<int> side_controls = {
        kIdCameraPanelCard,
        kIdImagePanelCard,
        kIdFluorescencePanelCard,
        kIdProcessingPanelCard,
        kIdMeasurementPanelCard,
        kIdProjectPanelCard,
        kIdAutoExposure,
        kIdCameraExposureLabel,
        kIdCameraExposureEdit,
        kIdCameraExposureApply,
        kIdCameraGainLabel,
        kIdCameraGainEdit,
        kIdCameraGainApply,
        kIdWhiteBalance,
        kIdPseudoColorLabel,
        kIdPseudoColorCombo,
        kIdDyeLabel,
        kIdDyeCombo,
        kIdDyeNameLabel,
        kIdDyeNameEdit,
        kIdDyeExcitationLabel,
        kIdDyeExcitationEdit,
        kIdDyeEmissionLabel,
        kIdDyeEmissionEdit,
        kIdDyeRedLabel,
        kIdDyeRedEdit,
        kIdDyeGreenLabel,
        kIdDyeGreenEdit,
        kIdDyeBlueLabel,
        kIdDyeBlueEdit,
        kIdSaveDye,
        kIdDeleteDye,
        kIdAddChannel,
        kIdFusionPreview,
        kIdClearChannels,
        kIdChannelLabel,
        kIdChannelList,
        kIdChannelVisible,
        kIdChannelBlackLabel,
        kIdChannelBlackEdit,
        kIdChannelWhiteLabel,
        kIdChannelWhiteEdit,
        kIdApplyChannel,
        kIdStitchSearchLabel,
        kIdStitchSearchEdit,
        kIdAddStitchTile,
        kIdBuildStitch,
        kIdEdfRadiusLabel,
        kIdEdfRadiusEdit,
        kIdAddEdfFrame,
        kIdBuildEdf,
        kIdShowEdfComposite,
        kIdShowEdfFocusMap,
        kIdRetryProcessing,
        kIdClearProcessing,
        kIdCalibrationLengthLabel,
        kIdCalibrationLengthEdit,
        kIdCalibrationUnitCombo,
        kIdCalibrate,
        kIdLengthTool,
        kIdAngleTool,
        kIdRectangleAreaTool,
        kIdPolygonAreaTool,
        kIdFinishPolygonArea,
        kIdDeleteMeasurement,
        kIdClearMeasurements,
        kIdExportCsv,
        kIdExportImage,
        kIdOpenImage,
        kIdSaveDiagnostics,
        kIdOpenProject,
        kIdSaveProject,
        kIdMeasurementNameEdit,
        kIdRenameMeasurement,
        kIdResultsLabel,
        kIdResultsList
    };
    return side_controls;
}
