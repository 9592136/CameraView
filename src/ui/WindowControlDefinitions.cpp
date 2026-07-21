#include "WindowControlDefinitions.h"

#include "ControlIds.h"

#include <algorithm>
#include <commctrl.h>

namespace {

constexpr DWORD kChildStyle = WS_CHILD | WS_VISIBLE;
constexpr DWORD kStaticStyle = kChildStyle;
constexpr DWORD kButtonStyle = kChildStyle | BS_PUSHBUTTON;
constexpr DWORD kPanelHeaderStyle = kChildStyle | BS_OWNERDRAW;
constexpr DWORD kEditStyle = kChildStyle | WS_BORDER | ES_AUTOHSCROLL;
constexpr DWORD kNumberEditStyle = kChildStyle | WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL;
constexpr DWORD kComboStyle = kChildStyle | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL;
constexpr DWORD kCheckboxStyle = kChildStyle | WS_TABSTOP | BS_AUTOCHECKBOX;
constexpr DWORD kListStyle = kChildStyle | WS_BORDER | WS_VSCROLL | LBS_NOINTEGRALHEIGHT;
constexpr DWORD kPanelScrollStyle = kChildStyle | SBS_VERT;
constexpr DWORD kTrackbarStyle = kChildStyle | TBS_HORZ | TBS_AUTOTICKS | TBS_BOTTOM | WS_TABSTOP;
constexpr DWORD kSmallStaticStyle = kChildStyle | SS_CENTER;

} // namespace

const std::vector<WindowControlDefinition>& WindowControlDefinitions::All()
{
    static const std::vector<WindowControlDefinition> definitions = {
        {kIdDeviceLabel, L"STATIC", L"Device", kStaticStyle},
        {kIdDeviceCombo, L"COMBOBOX", nullptr, kComboStyle},
        {kIdRefreshDevices, L"BUTTON", L"Refresh", kButtonStyle},
        {kIdOpen, L"BUTTON", L"Open", kButtonStyle},
        {kIdStop, L"BUTTON", L"Stop", kButtonStyle},
        {kIdFitView, L"BUTTON", L"Fit", kButtonStyle},
        {kIdExposureLabel, L"STATIC", L"Exposure", kStaticStyle},
        {kIdExposureEdit, L"EDIT", L"10", kEditStyle},
        {kIdExposureApply, L"BUTTON", L"Apply", kButtonStyle},
        {kIdObjectiveLabel, L"STATIC", L"Objective", kStaticStyle},
        {kIdObjectiveCombo, L"COMBOBOX", nullptr, kComboStyle},
        {kIdObjectiveNameEdit, L"EDIT", nullptr, kEditStyle},
        {kIdAddObjective, L"BUTTON", L"Add Obj", kButtonStyle},
        {kIdRenameObjective, L"BUTTON", L"Rename", kButtonStyle},
        {kIdDeleteObjective, L"BUTTON", L"Delete", kButtonStyle},
        {kIdCalibrationLengthLabel, L"STATIC", L"Scale length", kStaticStyle},
        {kIdCalibrationLengthEdit, L"EDIT", L"100", kEditStyle},
        {kIdCalibrationUnitCombo, L"COMBOBOX", nullptr, kComboStyle},
        {kIdCalibrationStatusLabel, L"STATIC", L"Current scale: uncalibrated", kStaticStyle},
        {kIdCalibrate, L"BUTTON", L"Calibrate", kButtonStyle},
        {kIdClearCalibration, L"BUTTON", L"Clear Calib", kButtonStyle},
        {kIdLengthTool, L"BUTTON", L"Length", kButtonStyle},
        {kIdAngleTool, L"BUTTON", L"Angle", kButtonStyle},
        {kIdRectangleAreaTool, L"BUTTON", L"Area", kButtonStyle},
        {kIdPolygonAreaTool, L"BUTTON", L"Poly Area", kButtonStyle},
        {kIdFinishPolygonArea, L"BUTTON", L"Finish Poly", kButtonStyle},
        {kIdDeleteMeasurement, L"BUTTON", L"Delete Selected", kButtonStyle},
        {kIdClearMeasurements, L"BUTTON", L"Clear", kButtonStyle},
        {kIdExportCsv, L"BUTTON", L"Export CSV", kButtonStyle},
        {kIdExportImage, L"BUTTON", L"Export Image", kButtonStyle},
        {kIdOpenImage, L"BUTTON", L"Open Image", kButtonStyle},
        {kIdToggleFunctionPanel, L"BUTTON", L"Hide Panel", kButtonStyle},
        {kIdTogglePanelDock, L"BUTTON", L"Dock Right", kButtonStyle},
        {kIdSaveDiagnostics, L"BUTTON", L"Save Report", kButtonStyle},
        {kIdDesignReportTemplate, L"BUTTON", L"Design Template", kButtonStyle},
        {kIdLoadReportTemplate, L"BUTTON", L"Load Template", kButtonStyle},
        {kIdClearReportTemplate, L"BUTTON", L"Clear Template", kButtonStyle},
        {kIdReportTemplateStatus, L"STATIC", L"Template: default", kStaticStyle},
        {kIdOpenProject, L"BUTTON", L"Open Project", kButtonStyle},
        {kIdSaveProject, L"BUTTON", L"Save Project", kButtonStyle},
        {kIdCameraPanelCard, L"BUTTON", L"Camera", kPanelHeaderStyle},
        {kIdImagePanelCard, L"BUTTON", L"Image", kPanelHeaderStyle},
        {kIdFluorescencePanelCard, L"BUTTON", L"Fluorescence", kPanelHeaderStyle},
        {kIdProcessingPanelCard, L"BUTTON", L"Processing", kPanelHeaderStyle},
        {kIdMeasurementPanelCard, L"BUTTON", L"Measurement", kPanelHeaderStyle},
        {kIdProjectPanelCard, L"BUTTON", L"Project", kPanelHeaderStyle},
        {kIdHistogramPanelCard, L"BUTTON", L"Histogram", kPanelHeaderStyle},
        {kIdAutoExposure, L"BUTTON", L"Auto Exposure", kButtonStyle},
        {kIdCameraExposureLabel, L"STATIC", L"Exposure", kStaticStyle},
        {kIdCameraExposureEdit, L"EDIT", L"10", kEditStyle},
        {kIdCameraExposureApply, L"BUTTON", L"Apply", kButtonStyle},
        {kIdCameraGainLabel, L"STATIC", L"Gain", kStaticStyle},
        {kIdCameraGainEdit, L"EDIT", L"1.0", kEditStyle},
        {kIdCameraGainApply, L"BUTTON", L"Apply", kButtonStyle},
        {kIdWhiteBalance, L"BUTTON", L"White Balance", kButtonStyle},
        {kIdHistogramChannelLabel, L"STATIC", L"Channel", kStaticStyle},
        {kIdHistogramChannelCombo, L"COMBOBOX", nullptr, kComboStyle},
        {kIdHistogramBrightnessLabel, L"STATIC", L"Brightness", kStaticStyle},
        {kIdHistogramBrightnessSlider, TRACKBAR_CLASS, nullptr, kTrackbarStyle},
        {kIdHistogramBrightnessValue, L"STATIC", L"0", kSmallStaticStyle},
        {kIdHistogramContrastLabel, L"STATIC", L"Contrast", kStaticStyle},
        {kIdHistogramContrastSlider, TRACKBAR_CLASS, nullptr, kTrackbarStyle},
        {kIdHistogramContrastValue, L"STATIC", L"0", kSmallStaticStyle},
        {kIdHistogramGammaLabel, L"STATIC", L"Gamma", kStaticStyle},
        {kIdHistogramGammaSlider, TRACKBAR_CLASS, nullptr, kTrackbarStyle},
        {kIdHistogramGammaValue, L"STATIC", L"1.0", kSmallStaticStyle},
        {kIdHistogramResetAdjust, L"BUTTON", L"Reset", kButtonStyle},
        {kIdHistogramWindowLevelLabel, L"STATIC", L"Window Level", kStaticStyle},
        {kIdHistogramWindowLevelSlider, TRACKBAR_CLASS, nullptr, kTrackbarStyle},
        {kIdHistogramWindowLevelValue, L"STATIC", L"128", kSmallStaticStyle},
        {kIdHistogramWindowWidthLabel, L"STATIC", L"Window Width", kStaticStyle},
        {kIdHistogramWindowWidthSlider, TRACKBAR_CLASS, nullptr, kTrackbarStyle},
        {kIdHistogramWindowWidthValue, L"STATIC", L"256", kSmallStaticStyle},
        {kIdPseudoColorLabel, L"STATIC", L"Pseudo color", kStaticStyle},
        {kIdPseudoColorCombo, L"COMBOBOX", nullptr, kComboStyle},
        {kIdDyeLabel, L"STATIC", L"Dye", kStaticStyle},
        {kIdDyeCombo, L"COMBOBOX", nullptr, kComboStyle},
        {kIdDyeNameLabel, L"STATIC", L"Name", kStaticStyle},
        {kIdDyeNameEdit, L"EDIT", nullptr, kEditStyle},
        {kIdDyeExcitationLabel, L"STATIC", L"Ex", kStaticStyle},
        {kIdDyeExcitationEdit, L"EDIT", nullptr, kEditStyle},
        {kIdDyeEmissionLabel, L"STATIC", L"Em", kStaticStyle},
        {kIdDyeEmissionEdit, L"EDIT", nullptr, kEditStyle},
        {kIdDyeRedLabel, L"STATIC", L"R", kStaticStyle},
        {kIdDyeRedEdit, L"EDIT", nullptr, kNumberEditStyle},
        {kIdDyeGreenLabel, L"STATIC", L"G", kStaticStyle},
        {kIdDyeGreenEdit, L"EDIT", nullptr, kNumberEditStyle},
        {kIdDyeBlueLabel, L"STATIC", L"B", kStaticStyle},
        {kIdDyeBlueEdit, L"EDIT", nullptr, kNumberEditStyle},
        {kIdSaveDye, L"BUTTON", L"Save Dye", kButtonStyle},
        {kIdDeleteDye, L"BUTTON", L"Delete Dye", kButtonStyle},
        {kIdAddChannel, L"BUTTON", L"Add Channel", kButtonStyle},
        {kIdFusionPreview, L"BUTTON", L"Fusion Preview", kCheckboxStyle},
        {kIdClearChannels, L"BUTTON", L"Clear Channels", kButtonStyle},
        {kIdChannelLabel, L"STATIC", L"Channels", kStaticStyle},
        {kIdChannelList, L"LISTBOX", nullptr, kListStyle},
        {kIdChannelVisible, L"BUTTON", L"Visible", kCheckboxStyle},
        {kIdChannelBlackLabel, L"STATIC", L"Black", kStaticStyle},
        {kIdChannelBlackEdit, L"EDIT", L"0", kNumberEditStyle},
        {kIdChannelWhiteLabel, L"STATIC", L"White", kStaticStyle},
        {kIdChannelWhiteEdit, L"EDIT", L"255", kNumberEditStyle},
        {kIdApplyChannel, L"BUTTON", L"Apply Channel", kButtonStyle},
        {kIdStitchSearchLabel, L"STATIC", L"Stitch search %", kStaticStyle},
        {kIdStitchSearchEdit, L"EDIT", L"85", kNumberEditStyle},
        {kIdAddStitchTile, L"BUTTON", L"Add Tile", kButtonStyle},
        {kIdBuildStitch, L"BUTTON", L"Stitch", kButtonStyle},
        {kIdStitchTileLabel, L"STATIC", L"Stitch tiles", kStaticStyle},
        {kIdStitchTileList, L"LISTBOX", nullptr, kListStyle},
        {kIdDeleteStitchTile, L"BUTTON", L"Delete Tile", kButtonStyle},
        {kIdClearStitchTiles, L"BUTTON", L"Clear Tiles", kButtonStyle},
        {kIdEdfRadiusLabel, L"STATIC", L"EDF radius", kStaticStyle},
        {kIdEdfRadiusEdit, L"EDIT", L"1", kNumberEditStyle},
        {kIdAddEdfFrame, L"BUTTON", L"Add EDF", kButtonStyle},
        {kIdBuildEdf, L"BUTTON", L"EDF", kButtonStyle},
        {kIdShowEdfComposite, L"BUTTON", L"EDF Image", kButtonStyle},
        {kIdShowEdfFocusMap, L"BUTTON", L"Focus Map", kButtonStyle},
        {kIdRetryProcessing, L"BUTTON", L"Retry", kButtonStyle},
        {kIdClearProcessing, L"BUTTON", L"Clear Processing", kButtonStyle},
        {kIdMeasurementNameEdit, L"EDIT", nullptr, kEditStyle},
        {kIdRenameMeasurement, L"BUTTON", L"Rename", kButtonStyle},
        {kIdResultsLabel, L"STATIC", L"Measurements", kStaticStyle},
        {kIdResultsList, L"LISTBOX", nullptr, kListStyle},
        {kIdPanelScrollBar, L"SCROLLBAR", nullptr, kPanelScrollStyle}
    };
    return definitions;
}

const WindowControlDefinition* WindowControlDefinitions::Find(int control_id)
{
    const std::vector<WindowControlDefinition>& definitions = All();
    const auto found = std::find_if(
        definitions.begin(),
        definitions.end(),
        [&](const WindowControlDefinition& definition) {
            return definition.control_id == control_id;
        });
    return found == definitions.end() ? nullptr : &*found;
}
