#include "WindowControlLayout.h"

#include "ControlIds.h"
#include "WindowLayout.h"

#include <algorithm>

namespace {

constexpr int kControlGap = 8;
constexpr int kToolbarTop = 10;
constexpr int kControlHeight = 28;
constexpr int kPanelMargin = 12;
constexpr int kPanelTitleHeight = 34;
constexpr int kPanelHeaderHeight = 30;
constexpr int kPanelHeaderGap = 3;
constexpr int kContentInset = 8;
constexpr int kPanelScrollBarWidth = 18;

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

    Add(placements, kIdFitView, left, kToolbarTop, 56, kControlHeight);
    left += 56 + kControlGap;
    Add(placements, kIdToggleFunctionPanel, left, kToolbarTop, 96, kControlHeight);
    left += 96 + kControlGap;
    Add(placements, kIdTogglePanelDock, left, kToolbarTop, 88, kControlHeight);
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

void AddSidePanel(
    std::vector<WindowControlPlacement>& placements,
    const RECT& panel,
    int panel_category,
    int panel_scroll_offset,
    int* content_bottom = nullptr,
    bool show_scroll_bar = false)
{
    const int header_x = static_cast<int>(panel.left) + 4;
    const int content_right =
        static_cast<int>(panel.right) - (show_scroll_bar ? kPanelScrollBarWidth : 0);
    const int header_w = std::max(80, content_right - static_cast<int>(panel.left) - 8);
    int y = static_cast<int>(panel.top) + kPanelTitleHeight + 5;
    const int logical_top = y;
    const int viewport_top = static_cast<int>(panel.top) + kPanelTitleHeight + 1;
    const int viewport_bottom = static_cast<int>(panel.bottom);
    panel_scroll_offset = std::max(0, panel_scroll_offset);
    const int selected_category = WindowControlLayout::NormalizePanelCategory(panel_category);
    std::vector<int> visible_control_ids;
    if (show_scroll_bar) {
        placements.push_back(WindowControlPlacement{
            kIdPanelScrollBar,
            RECT{
                content_right,
                static_cast<int>(panel.top) + kPanelTitleHeight + 1,
                static_cast<int>(panel.right),
                static_cast<int>(panel.bottom)
            },
            true
        });
        visible_control_ids.push_back(kIdPanelScrollBar);
    }
    auto add = [&](int control_id, int left, int top, int width, int height) {
        RECT bounds{
            left,
            top - panel_scroll_offset,
            left + width,
            top - panel_scroll_offset + height
        };
        const bool visible =
            bounds.top >= viewport_top &&
            bounds.bottom <= viewport_bottom;
        placements.push_back(WindowControlPlacement{
            control_id,
            visible ? bounds : RECT{0, 0, 0, 0},
            visible
        });
        visible_control_ids.push_back(control_id);
    };

    auto add_content = [&](int category, int& content_y, int bottom_limit) {
        const int x = header_x + kContentInset;
        const int w = std::max(80, header_w - kContentInset * 2);
        const int half_width = std::max(40, (w - kControlGap) / 2);
        const int second_column = x + half_width + kControlGap;
        switch (category) {
    case 1:
        add(kIdPseudoColorLabel, x, content_y, w, 20);
        content_y += 24;
        add(kIdPseudoColorCombo, x, content_y, w, 140);
        content_y += kControlHeight + kControlGap;
        add(kIdOpenImage, x, content_y, half_width, kControlHeight);
        add(kIdExportImage, second_column, content_y, half_width, kControlHeight);
        content_y += kControlHeight;
        break;
    case 2:
        add(kIdDyeLabel, x, content_y, w, 20);
        content_y += 24;
        add(kIdDyeCombo, x, content_y, w, 140);
        content_y += kControlHeight + kControlGap;
        add(kIdDyeNameLabel, x, content_y + 5, 42, 20);
        add(kIdDyeNameEdit, x + 48, content_y, w - 48, kControlHeight);
        content_y += kControlHeight + kControlGap;
        add(kIdDyeExcitationLabel, x, content_y + 5, 22, 20);
        add(kIdDyeExcitationEdit, x + 26, content_y, 54, kControlHeight);
        add(kIdDyeEmissionLabel, x + 88, content_y + 5, 24, 20);
        add(kIdDyeEmissionEdit, x + 116, content_y, 54, kControlHeight);
        content_y += kControlHeight + kControlGap;
        add(kIdDyeRedLabel, x, content_y + 5, 16, 20);
        add(kIdDyeRedEdit, x + 20, content_y, 38, kControlHeight);
        add(kIdDyeGreenLabel, x + 64, content_y + 5, 18, 20);
        add(kIdDyeGreenEdit, x + 84, content_y, 38, kControlHeight);
        add(kIdDyeBlueLabel, x + 128, content_y + 5, 18, 20);
        add(kIdDyeBlueEdit, x + 148, content_y, 38, kControlHeight);
        content_y += kControlHeight + kControlGap;
        add(kIdSaveDye, x, content_y, half_width, kControlHeight);
        add(kIdDeleteDye, second_column, content_y, half_width, kControlHeight);
        content_y += kControlHeight + kControlGap;
        add(kIdAddChannel, x, content_y, half_width, kControlHeight);
        add(kIdFusionPreview, second_column, content_y, half_width, kControlHeight);
        content_y += kControlHeight + kControlGap;
        add(kIdClearChannels, x, content_y, w, kControlHeight);
        content_y += kControlHeight + kControlGap;
        add(kIdChannelLabel, x, content_y, w, 20);
        content_y += 24;
        add(kIdChannelList, x, content_y, w, 96);
        content_y += 96 + kControlGap;
        add(kIdChannelVisible, x, content_y, half_width, kControlHeight);
        add(kIdApplyChannel, second_column, content_y, half_width, kControlHeight);
        content_y += kControlHeight + kControlGap;
        add(kIdChannelBlackLabel, x, content_y + 5, 34, 20);
        add(kIdChannelBlackEdit, x + 40, content_y, 48, kControlHeight);
        add(kIdChannelWhiteLabel, x + 96, content_y + 5, 38, 20);
        add(kIdChannelWhiteEdit, x + 140, content_y, 48, kControlHeight);
        content_y += kControlHeight;
        break;
    case 3:
        add(kIdStitchSearchLabel, x, content_y + 5, 98, 20);
        add(kIdStitchSearchEdit, x + 106, content_y, 54, kControlHeight);
        content_y += kControlHeight + kControlGap;
        add(kIdAddStitchTile, x, content_y, half_width, kControlHeight);
        add(kIdBuildStitch, second_column, content_y, half_width, kControlHeight);
        content_y += kControlHeight + kControlGap;
        add(kIdStitchTileLabel, x, content_y, w, 20);
        content_y += 24;
        {
            const int controls_after_gallery =
                (kControlHeight + kControlGap) +
                (kControlHeight + kControlGap) +
                (kControlHeight + kControlGap) +
                (kControlHeight + kControlGap) +
                kControlHeight;
            const int available_gallery_height =
                bottom_limit - content_y - controls_after_gallery - 8;
            const int gallery_height = std::clamp(available_gallery_height, 54, 96);
            add(kIdStitchTileList, x, content_y, w, gallery_height);
            content_y += gallery_height + kControlGap;
        }
        add(kIdDeleteStitchTile, x, content_y, half_width, kControlHeight);
        add(kIdClearStitchTiles, second_column, content_y, half_width, kControlHeight);
        content_y += kControlHeight + kControlGap + 8;
        add(kIdEdfRadiusLabel, x, content_y + 5, 76, 20);
        add(kIdEdfRadiusEdit, x + 84, content_y, 54, kControlHeight);
        content_y += kControlHeight + kControlGap;
        add(kIdAddEdfFrame, x, content_y, half_width, kControlHeight);
        add(kIdBuildEdf, second_column, content_y, half_width, kControlHeight);
        content_y += kControlHeight + kControlGap;
        add(kIdShowEdfComposite, x, content_y, half_width, kControlHeight);
        add(kIdShowEdfFocusMap, second_column, content_y, half_width, kControlHeight);
        content_y += kControlHeight + kControlGap;
        add(kIdRetryProcessing, x, content_y, half_width, kControlHeight);
        add(kIdClearProcessing, second_column, content_y, half_width, kControlHeight);
        content_y += kControlHeight;
        break;
    case 4:
        add(kIdObjectiveLabel, x, content_y, w, 20);
        content_y += 24;
        add(kIdObjectiveCombo, x, content_y, w, 120);
        content_y += kControlHeight + kControlGap;
        add(kIdObjectiveNameEdit, x, content_y, w, kControlHeight);
        content_y += kControlHeight + kControlGap;
        {
            const int button_gap = 6;
            const int third_width = (w - button_gap * 2) / 3;
            add(kIdAddObjective, x, content_y, third_width, kControlHeight);
            add(kIdRenameObjective, x + third_width + button_gap, content_y, third_width, kControlHeight);
            add(kIdDeleteObjective, x + (third_width + button_gap) * 2, content_y, w - (third_width + button_gap) * 2, kControlHeight);
            content_y += kControlHeight + kControlGap;
        }
        add(kIdCalibrationLengthLabel, x, content_y, w, 20);
        content_y += 24;
        add(kIdCalibrationLengthEdit, x, content_y, 90, kControlHeight);
        add(kIdCalibrationUnitCombo, x + 98, content_y, 92, 100);
        content_y += kControlHeight + kControlGap;
        add(kIdCalibrationStatusLabel, x, content_y, w, 20);
        content_y += 24;
        add(kIdCalibrate, x, content_y, half_width, kControlHeight);
        add(kIdClearCalibration, second_column, content_y, half_width, kControlHeight);
        content_y += kControlHeight + kControlGap;
        add(kIdLengthTool, x, content_y, half_width, kControlHeight);
        add(kIdAngleTool, second_column, content_y, half_width, kControlHeight);
        content_y += kControlHeight + kControlGap;
        add(kIdRectangleAreaTool, x, content_y, half_width, kControlHeight);
        add(kIdPolygonAreaTool, second_column, content_y, half_width, kControlHeight);
        content_y += kControlHeight + kControlGap;
        add(kIdFinishPolygonArea, x, content_y, half_width, kControlHeight);
        add(kIdDeleteMeasurement, second_column, content_y, half_width, kControlHeight);
        content_y += kControlHeight + kControlGap;
        add(kIdClearMeasurements, x, content_y, half_width, kControlHeight);
        add(kIdExportCsv, second_column, content_y, half_width, kControlHeight);
        content_y += kControlHeight + kControlGap;
        add(kIdMeasurementNameEdit, x, content_y, w - 64, kControlHeight);
        add(kIdRenameMeasurement, x + w - 56, content_y, 56, kControlHeight);
        content_y += kControlHeight + kControlGap + 8;
        add(kIdResultsLabel, x, content_y, w, 20);
        content_y += 24;
        add(kIdResultsList, x, content_y, w, std::max(0, bottom_limit - content_y));
        content_y = bottom_limit;
        break;
    case 5:
        add(kIdOpenProject, x, content_y, half_width, kControlHeight);
        add(kIdSaveProject, second_column, content_y, half_width, kControlHeight);
        content_y += kControlHeight + kControlGap;
        add(kIdDesignReportTemplate, x, content_y, w, kControlHeight);
        content_y += kControlHeight + kControlGap;
        add(kIdLoadReportTemplate, x, content_y, half_width, kControlHeight);
        add(kIdClearReportTemplate, second_column, content_y, half_width, kControlHeight);
        content_y += kControlHeight + kControlGap;
        add(kIdReportTemplateStatus, x, content_y, w, 20);
        content_y += 24;
        add(kIdSaveDiagnostics, x, content_y, w, kControlHeight);
        content_y += kControlHeight;
        break;
    case 0:
    default:
        add(kIdDeviceLabel, x, content_y, w, 20);
        content_y += 24;
        add(kIdDeviceCombo, x, content_y, w, 140);
        content_y += kControlHeight + kControlGap;
        add(kIdRefreshDevices, x, content_y, half_width, kControlHeight);
        add(kIdOpen, second_column, content_y, half_width, kControlHeight);
        content_y += kControlHeight + kControlGap;
        add(kIdStop, x, content_y, half_width, kControlHeight);
        add(kIdAutoExposure, second_column, content_y, half_width, kControlHeight);
        content_y += kControlHeight + kControlGap;
        add(kIdCameraExposureLabel, x, content_y + 5, 74, 20);
        add(kIdCameraExposureEdit, x + 82, content_y, 92, kControlHeight);
        add(kIdCameraExposureApply, second_column, content_y, half_width, kControlHeight);
        content_y += kControlHeight + kControlGap;
        add(kIdCameraGainLabel, x, content_y + 5, 74, 20);
        add(kIdCameraGainEdit, x + 82, content_y, 92, kControlHeight);
        add(kIdCameraGainApply, second_column, content_y, half_width, kControlHeight);
        content_y += kControlHeight + kControlGap;
        add(kIdWhiteBalance, x, content_y, w, kControlHeight);
        content_y += kControlHeight;
        break;
        }
    };

    const int header_ids[] = {
        kIdCameraPanelCard,
        kIdImagePanelCard,
        kIdFluorescencePanelCard,
        kIdProcessingPanelCard,
        kIdMeasurementPanelCard,
        kIdProjectPanelCard
    };
    constexpr int kHeaderCount = 6;
    for (int category = 0; category < kHeaderCount; ++category) {
        add(header_ids[category], header_x, y, header_w, kPanelHeaderHeight);
        y += kPanelHeaderHeight;
        if (category == selected_category) {
            y += 8;
            const int bottom_limit =
                static_cast<int>(panel.bottom) -
                kPanelMargin;
            add_content(category, y, bottom_limit);
            y += 8;
        } else {
            y += kPanelHeaderGap;
        }
    }

    if (content_bottom) {
        *content_bottom = std::max(0, y - logical_top);
    }
    AddHiddenSideControlsExcept(placements, visible_control_ids);
}

} // namespace

std::vector<WindowControlPlacement> WindowControlLayout::Compute(
    const RECT& client_rect,
    int panel_category,
    int panel_scroll_offset,
    bool show_side_panel,
    bool dock_panel_left)
{
    std::vector<WindowControlPlacement> placements;
    placements.reserve(3 + SideControlIds().size());
    AddToolbar(placements);

    const RECT panel = WindowLayout::SidePanelRect(client_rect, show_side_panel, dock_panel_left);
    if (panel.right <= panel.left) {
        AddHiddenSideControls(placements);
        return placements;
    }

    const bool show_scroll_bar =
        PanelScrollMax(client_rect, panel_category, show_side_panel, dock_panel_left) > 0;
    AddSidePanel(placements, panel, panel_category, panel_scroll_offset, nullptr, show_scroll_bar);
    return placements;
}

int WindowControlLayout::PanelScrollMax(
    const RECT& client_rect,
    int panel_category,
    bool show_side_panel,
    bool dock_panel_left)
{
    const RECT panel = WindowLayout::SidePanelRect(client_rect, show_side_panel, dock_panel_left);
    if (panel.right <= panel.left || panel.bottom <= panel.top) {
        return 0;
    }

    std::vector<WindowControlPlacement> placements;
    int content_height = 0;
    AddSidePanel(placements, panel, panel_category, 0, &content_height, true);
    const int viewport_height = PanelScrollPage(client_rect, show_side_panel, dock_panel_left);
    return std::max(0, content_height - viewport_height);
}

int WindowControlLayout::PanelScrollPage(
    const RECT& client_rect,
    bool show_side_panel,
    bool dock_panel_left)
{
    const RECT panel = WindowLayout::SidePanelRect(client_rect, show_side_panel, dock_panel_left);
    if (panel.right <= panel.left || panel.bottom <= panel.top) {
        return 0;
    }
    return std::max(0, static_cast<int>(panel.bottom - panel.top) - kPanelTitleHeight - 1);
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
        kIdDeviceLabel,
        kIdDeviceCombo,
        kIdRefreshDevices,
        kIdOpen,
        kIdStop,
        kIdExposureLabel,
        kIdExposureEdit,
        kIdExposureApply,
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
        kIdStitchTileLabel,
        kIdStitchTileList,
        kIdDeleteStitchTile,
        kIdClearStitchTiles,
        kIdEdfRadiusLabel,
        kIdEdfRadiusEdit,
        kIdAddEdfFrame,
        kIdBuildEdf,
        kIdShowEdfComposite,
        kIdShowEdfFocusMap,
        kIdRetryProcessing,
        kIdClearProcessing,
        kIdObjectiveLabel,
        kIdObjectiveCombo,
        kIdObjectiveNameEdit,
        kIdAddObjective,
        kIdRenameObjective,
        kIdDeleteObjective,
        kIdCalibrationLengthLabel,
        kIdCalibrationLengthEdit,
        kIdCalibrationUnitCombo,
        kIdCalibrationStatusLabel,
        kIdCalibrate,
        kIdClearCalibration,
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
        kIdDesignReportTemplate,
        kIdLoadReportTemplate,
        kIdClearReportTemplate,
        kIdReportTemplateStatus,
        kIdOpenProject,
        kIdSaveProject,
        kIdMeasurementNameEdit,
        kIdRenameMeasurement,
        kIdResultsLabel,
        kIdResultsList,
        kIdPanelScrollBar
    };
    return side_controls;
}
