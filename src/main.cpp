#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>

#include "app/DiagnosticReportActions.h"
#include "app/ExportActions.h"
#include "app/ProjectActions.h"
#include "app/ProjectSessionRestorer.h"
#include "camera/CameraControlStatusFormatter.h"
#include "camera/CameraDevice.h"
#include "camera/CameraDeviceListFormatter.h"
#include "camera/CameraTelemetryFormatter.h"
#include "camera/FrameBuffer.h"
#include "camera/MUCamCameraDriver.h"
#include "domain/CalibrationProfile.h"
#include "domain/ImageFrame.h"
#include "domain/Measurement.h"
#include "domain/MeasurementCollection.h"
#include "domain/MeasurementNameFormatter.h"
#include "imaging/ChannelFusionEngine.h"
#include "imaging/DyeLibrary.h"
#include "imaging/EdfProcessor.h"
#include "imaging/FluorescenceChannelListActions.h"
#include "imaging/FluorescenceChannelUpdater.h"
#include "imaging/ImageStitcher.h"
#include "imaging/LiveStitchCapturePlanner.h"
#include "imaging/LiveStitchPreviewBuilder.h"
#include "imaging/ImageViewport.h"
#include "imaging/OverlayRenderer.h"
#include "imaging/PreviewDisplayActions.h"
#include "imaging/ProcessingParameterRules.h"
#include "imaging/ProcessingPanelActions.h"
#include "imaging/ProcessingJobState.h"
#include "imaging/ProcessingRetryActions.h"
#include "imaging/ProcessingStartActions.h"
#include "imaging/ProcessingResultActions.h"
#include "imaging/ProcessingResultFrames.h"
#include "imaging/ProcessingRetryState.h"
#include "imaging/ProcessingWorkerActions.h"
#include "imaging/StitchTileListActions.h"
#include "imaging/ViewportInteractionActions.h"
#include "platform/FileDialog.h"
#include "storage/ImageExporter.h"
#include "platform/TextInputParser.h"
#include "ui/ControlIds.h"
#include "ui/CameraPanelActions.h"
#include "ui/DyeLibraryActions.h"
#include "ui/DyeProfileFormPresenter.h"
#include "ui/FluorescenceDisplayActions.h"
#include "ui/FluorescenceChannelFormPresenter.h"
#include "ui/MeasurementDisplayActions.h"
#include "ui/MeasurementEditSession.h"
#include "ui/MeasurementHitTester.h"
#include "ui/MeasurementInteractionActions.h"
#include "ui/MeasurementInteractionState.h"
#include "ui/MeasurementListActions.h"
#include "ui/MeasurementToolAvailability.h"
#include "ui/MeasurementToolStartActions.h"
#include "ui/ProcessingBuildInputActions.h"
#include "ui/ProcessingQueueActions.h"
#include "ui/StitchTileDisplayActions.h"
#include "ui/WindowControlDefinitions.h"
#include "ui/WindowControlLayout.h"
#include "ui/WindowLayout.h"
#include "imaging/HistogramCalculator.h"
#include "ui/HistogramRenderer.h"
#include "imaging/ImageAdjuster.h"
#include "i18n/Localization.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr UINT kMsgFrameReady = WM_APP + 1;
constexpr UINT kMsgStatusChanged = WM_APP + 2;
constexpr UINT kMsgProcessingFinished = WM_APP + 3;
constexpr UINT kMsgLiveStitchPreviewReady = WM_APP + 4;
constexpr UINT kMsgLiveStitchCaptureReady = WM_APP + 5;
constexpr UINT_PTR kLiveStitchTimerId = 5001;
constexpr int kDefaultLiveStitchIntervalMs = 1200;
constexpr int kMinLiveStitchIntervalMs = 250;
constexpr int kMaxLiveStitchIntervalMs = 10000;
constexpr int kLiveStitchPreviewMaxEdge = 960;
constexpr int kLiveStitchPreviewTileMaxEdge = 640;
constexpr int kLiveStitchRegistrationMaxEdge = 224;
constexpr int kLiveStitchMinMovementPercent = 15;
constexpr int kLiveStitchMinOverlapPercent = 15;
constexpr int kLiveStitchReferenceTileCount = 5;
constexpr int kLiveStitchOutOfRangeWarningFrames = 3;
constexpr int kLiveStitchMissingMatchWarningFrames = 6;
constexpr DWORD kLiveStitchStatusMinIntervalMs = 1000;
constexpr DWORD kLiveStitchPreviewStatusMinIntervalMs = 700;
constexpr DWORD kLiveStitchWarningBeepMinIntervalMs = 2500;
constexpr DWORD kLivePreviewOverlayMinIntervalMs = 50;
constexpr int kLivePreviewOverlayMaxEdge = 420;
constexpr int kPanelTitleHeight = 34;
constexpr int kFunctionPanelResizeGripWidth = 8;
constexpr const wchar_t* kFunctionPanelVisibleProperty = L"CameraViewFunctionPanelVisible";
constexpr const wchar_t* kFunctionPanelDockLeftProperty = L"CameraViewFunctionPanelDockLeft";
constexpr const wchar_t* kFunctionPanelWidthProperty = L"CameraViewFunctionPanelWidth";
constexpr const wchar_t* kReportTemplateDesignerClassName = L"CameraViewReportTemplateDesigner";
constexpr const wchar_t* kReportTemplatePreviewClassName = L"CameraViewReportTemplatePreview";
constexpr int kReportTemplateDesignerDefaultWidth = 980;
constexpr int kReportTemplateDesignerDefaultHeight = 940;
constexpr int kReportTemplateDesignerMinWidth = 780;
constexpr int kReportTemplateDesignerMinHeight = 720;
constexpr INT_PTR kFunctionPanelVisibleValue = 1;
constexpr INT_PTR kFunctionPanelHiddenValue = 2;
constexpr INT_PTR kFunctionPanelDockLeftValue = 1;
constexpr INT_PTR kFunctionPanelDockRightValue = 2;
constexpr int kTemplateDesignerTitleLabel = 4101;
constexpr int kTemplateDesignerTitleEdit = 4102;
constexpr int kTemplateDesignerImage = 4103;
constexpr int kTemplateDesignerSummary = 4104;
constexpr int kTemplateDesignerTable = 4105;
constexpr int kTemplateDesignerCalibration = 4106;
constexpr int kTemplateDesignerProcessing = 4107;
constexpr int kTemplateDesignerFooter = 4108;
constexpr int kTemplateDesignerPreview = 4109;
constexpr int kTemplateDesignerStatus = 4110;
constexpr int kTemplateDesignerDefault = 4111;
constexpr int kTemplateDesignerApply = 4112;
constexpr int kTemplateDesignerSave = 4113;
constexpr int kTemplateDesignerClose = 4114;
constexpr int kTemplateDesignerSectionLabel = 4115;
constexpr int kTemplateDesignerSectionList = 4116;
constexpr int kTemplateDesignerMoveUp = 4117;
constexpr int kTemplateDesignerMoveDown = 4118;
constexpr int kTemplateDesignerNotes = 4119;
constexpr int kTemplateDesignerNotesLabel = 4120;
constexpr int kTemplateDesignerNotesEdit = 4121;
constexpr int kTemplateDesignerSubtitleLabel = 4122;
constexpr int kTemplateDesignerSubtitleEdit = 4123;
constexpr int kTemplateDesignerAccentLabel = 4124;
constexpr int kTemplateDesignerAccentBlue = 4125;
constexpr int kTemplateDesignerAccentGreen = 4126;
constexpr int kTemplateDesignerAccentGold = 4127;
constexpr int kTemplateDesignerAccentMagenta = 4128;
constexpr int kTemplateDesignerInfo = 4129;
constexpr int kTemplateDesignerInfoLabel = 4130;
constexpr int kTemplateDesignerInfoEdit = 4131;
constexpr int kTemplateDesignerImageSizeLabel = 4132;
constexpr int kTemplateDesignerImageSizeOriginal = 4133;
constexpr int kTemplateDesignerImageSizeFit = 4134;
constexpr int kTemplateDesignerImageSizeCompact = 4135;
constexpr int kTemplateDesignerRawValues = 4136;
constexpr int kTemplateDesignerGroupMeasurements = 4137;
constexpr int kTemplateDesignerImageCaptionLabel = 4138;
constexpr int kTemplateDesignerImageCaptionEdit = 4139;
constexpr int kTemplateDesignerSectionHeadingLabel = 4140;
constexpr int kTemplateDesignerSectionHeadingEdit = 4141;
constexpr int kTemplateDesignerFooterTextLabel = 4142;
constexpr int kTemplateDesignerFooterTextEdit = 4143;
constexpr int kTemplateDesignerInsertField = 4144;
constexpr int kTemplateDesignerPageLayoutLabel = 4145;
constexpr int kTemplateDesignerPageLayoutStandard = 4146;
constexpr int kTemplateDesignerPageLayoutWide = 4147;
constexpr int kTemplateDesignerPageLayoutCompact = 4148;
constexpr int kTemplateDesignerPrintOrientationLabel = 4149;
constexpr int kTemplateDesignerPrintOrientationPortrait = 4150;
constexpr int kTemplateDesignerPrintOrientationLandscape = 4151;
constexpr int kTemplateDesignerMeasurementPrecisionLabel = 4152;
constexpr int kTemplateDesignerMeasurementPrecisionAuto = 4153;
constexpr int kTemplateDesignerMeasurementPrecisionTwo = 4154;
constexpr int kTemplateDesignerMeasurementPrecisionThree = 4155;
constexpr int kTemplateDesignerLeftScrollBar = 4156;
constexpr int kTemplatePlaceholderMenuBase = 4300;

struct LiveStitchCaptureRequest {
    std::vector<LiveStitchPreviewTile> reference_tiles;
    std::shared_ptr<const ImageFrame> frame;
    LiveStitchCaptureOptions options;
    unsigned long long generation = 0;
    unsigned long long sequence = 0;
    std::size_t base_tile_count = 0;
};

struct LiveStitchCaptureResult {
    LiveStitchCaptureDecision decision;
    std::shared_ptr<const ImageFrame> frame;
    unsigned long long generation = 0;
    unsigned long long sequence = 0;
    std::size_t base_tile_count = 0;
    long long elapsed_ms = 0;
};

enum class PreviewFrameCacheKind {
    None,
    PseudoColor,
    Fusion
};

bool IsFunctionPanelVisible(HWND hwnd)
{
    const HANDLE value = GetPropW(hwnd, kFunctionPanelVisibleProperty);
    if (!value) {
        return true;
    }
    return value == reinterpret_cast<HANDLE>(kFunctionPanelVisibleValue);
}

bool IsFunctionPanelDockedLeft(HWND hwnd)
{
    const HANDLE value = GetPropW(hwnd, kFunctionPanelDockLeftProperty);
    if (!value) {
        return true;
    }
    return value == reinterpret_cast<HANDLE>(kFunctionPanelDockLeftValue);
}

int FunctionPanelWidth(HWND hwnd)
{
    return static_cast<int>(reinterpret_cast<INT_PTR>(GetPropW(hwnd, kFunctionPanelWidthProperty)));
}

void SetFunctionPanelWidthProperty(HWND hwnd, int width)
{
    if (width > 0) {
        SetPropW(
            hwnd,
            kFunctionPanelWidthProperty,
            reinterpret_cast<HANDLE>(static_cast<INT_PTR>(width)));
    } else {
        RemovePropW(hwnd, kFunctionPanelWidthProperty);
    }
}

void SetFunctionPanelVisibleProperty(HWND hwnd, bool visible)
{
    SetPropW(
        hwnd,
        kFunctionPanelVisibleProperty,
        reinterpret_cast<HANDLE>(visible ? kFunctionPanelVisibleValue : kFunctionPanelHiddenValue));
}

void SetFunctionPanelDockLeftProperty(HWND hwnd, bool dock_left)
{
    SetPropW(
        hwnd,
        kFunctionPanelDockLeftProperty,
        reinterpret_cast<HANDLE>(dock_left ? kFunctionPanelDockLeftValue : kFunctionPanelDockRightValue));
}

void RemoveFunctionPanelVisibleProperty(HWND hwnd)
{
    RemovePropW(hwnd, kFunctionPanelVisibleProperty);
}

void RemoveFunctionPanelDockLeftProperty(HWND hwnd)
{
    RemovePropW(hwnd, kFunctionPanelDockLeftProperty);
}

void RemoveFunctionPanelWidthProperty(HWND hwnd)
{
    RemovePropW(hwnd, kFunctionPanelWidthProperty);
}

// ---- Language property ---------------------------------------------------

constexpr const wchar_t* kLanguageProperty = L"CameraViewLanguage";
constexpr UINT_PTR kLanguageEnglishValue = 0;
constexpr UINT_PTR kLanguageChineseValue = 1;

void SetLanguageProperty(HWND hwnd, UILanguage lang)
{
    SetPropW(hwnd, kLanguageProperty,
             reinterpret_cast<HANDLE>(lang == UILanguage::Chinese ? kLanguageChineseValue : kLanguageEnglishValue));
}

UILanguage GetLanguageProperty(HWND hwnd)
{
    HANDLE value = GetPropW(hwnd, kLanguageProperty);
    return (value == reinterpret_cast<HANDLE>(kLanguageChineseValue)) ? UILanguage::Chinese : UILanguage::English;
}

HMENU CreateMainMenu(UILanguage lang)
{
    HMENU menu = CreateMenu();
    HMENU file_menu = CreatePopupMenu();
    HMENU view_menu = CreatePopupMenu();
    HMENU camera_menu = CreatePopupMenu();
    HMENU processing_menu = CreatePopupMenu();
    HMENU measurement_menu = CreatePopupMenu();
    HMENU language_menu = CreatePopupMenu();

    AppendMenuW(file_menu, MF_STRING, kIdOpenImage, GetLocStr(LocId::MF_OPEN_IMAGE, lang));
    AppendMenuW(file_menu, MF_STRING, kIdExportImage, GetLocStr(LocId::MF_EXPORT_IMAGE, lang));
    AppendMenuW(file_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file_menu, MF_STRING, kIdOpenProject, GetLocStr(LocId::MF_OPEN_PROJECT, lang));
    AppendMenuW(file_menu, MF_STRING, kIdSaveProject, GetLocStr(LocId::MF_SAVE_PROJECT, lang));
    AppendMenuW(file_menu, MF_STRING, kIdDesignReportTemplate, GetLocStr(LocId::MF_DESIGN_REPORT_TEMPLATE, lang));
    AppendMenuW(file_menu, MF_STRING, kIdSaveDiagnostics, GetLocStr(LocId::MF_SAVE_REPORT, lang));
    AppendMenuW(file_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file_menu, MF_STRING, kIdExit, GetLocStr(LocId::MF_EXIT, lang));

    AppendMenuW(view_menu, MF_STRING | MF_CHECKED, kIdToggleFunctionPanel, GetLocStr(LocId::MV_FUNCTION_PANEL, lang));
    AppendMenuW(view_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(view_menu, MF_STRING | MF_CHECKED, kIdDockFunctionPanelLeft, GetLocStr(LocId::MV_DOCK_LEFT, lang));
    AppendMenuW(view_menu, MF_STRING, kIdDockFunctionPanelRight, GetLocStr(LocId::MV_DOCK_RIGHT, lang));
    AppendMenuW(view_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(view_menu, MF_STRING, kIdFitView, GetLocStr(LocId::MV_FIT_IMAGE, lang));
    AppendMenuW(view_menu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(language_menu, MF_STRING | (lang == UILanguage::English ? MF_CHECKED : 0),
                kIdLanguageEnglish, GetLocStr(LocId::ML_ENGLISH, lang));
    AppendMenuW(language_menu, MF_STRING | (lang == UILanguage::Chinese ? MF_CHECKED : 0),
                kIdLanguageChinese, GetLocStr(LocId::ML_CHINESE, lang));
    AppendMenuW(view_menu, MF_POPUP, reinterpret_cast<UINT_PTR>(language_menu), GetLocStr(LocId::MENU_LANGUAGE, lang));

    AppendMenuW(camera_menu, MF_STRING, kIdRefreshDevices, GetLocStr(LocId::MC_REFRESH_DEVICES, lang));
    AppendMenuW(camera_menu, MF_STRING, kIdOpen, GetLocStr(LocId::MC_OPEN_CAMERA, lang));
    AppendMenuW(camera_menu, MF_STRING, kIdStop, GetLocStr(LocId::MC_STOP_CAMERA, lang));
    AppendMenuW(camera_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(camera_menu, MF_STRING, kIdAutoExposure, GetLocStr(LocId::MC_AUTO_EXPOSURE, lang));
    AppendMenuW(camera_menu, MF_STRING, kIdWhiteBalance, GetLocStr(LocId::MC_WHITE_BALANCE, lang));

    AppendMenuW(processing_menu, MF_STRING, kIdAddStitchTile, GetLocStr(LocId::MP_ADD_TILE, lang));
    AppendMenuW(processing_menu, MF_STRING, kIdStartLiveStitch, GetLocStr(LocId::MP_START_LIVE_STITCH, lang));
    AppendMenuW(processing_menu, MF_STRING, kIdStopLiveStitch, GetLocStr(LocId::MP_STOP_LIVE_STITCH, lang));
    AppendMenuW(processing_menu, MF_STRING, kIdBuildStitch, GetLocStr(LocId::MP_STITCH, lang));
    AppendMenuW(processing_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(processing_menu, MF_STRING, kIdAddEdfFrame, GetLocStr(LocId::MP_ADD_EDF_FRAME, lang));
    AppendMenuW(processing_menu, MF_STRING, kIdBuildEdf, GetLocStr(LocId::MP_BUILD_EDF, lang));
    AppendMenuW(processing_menu, MF_STRING, kIdClearProcessing, GetLocStr(LocId::MP_CLEAR_PROCESSING, lang));

    AppendMenuW(measurement_menu, MF_STRING, kIdCalibrate, GetLocStr(LocId::MM_CALIBRATE, lang));
    AppendMenuW(measurement_menu, MF_STRING, kIdClearCalibration, GetLocStr(LocId::MM_CLEAR_CALIBRATION, lang));
    AppendMenuW(measurement_menu, MF_STRING, kIdLengthTool, GetLocStr(LocId::MM_LENGTH, lang));
    AppendMenuW(measurement_menu, MF_STRING, kIdAngleTool, GetLocStr(LocId::MM_ANGLE, lang));
    AppendMenuW(measurement_menu, MF_STRING, kIdRectangleAreaTool, GetLocStr(LocId::MM_RECTANGLE_AREA, lang));
    AppendMenuW(measurement_menu, MF_STRING, kIdPolygonAreaTool, GetLocStr(LocId::MM_POLYGON_AREA, lang));
    AppendMenuW(measurement_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(measurement_menu, MF_STRING, kIdExportCsv, GetLocStr(LocId::MM_EXPORT_CSV, lang));

    // Help menu
    HMENU help_menu = CreatePopupMenu();
    AppendMenuW(help_menu, MF_STRING, kIdAbout, GetLocStr(LocId::MH_ABOUT, lang));

    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file_menu), GetLocStr(LocId::MENU_FILE, lang));
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(view_menu), GetLocStr(LocId::MENU_VIEW, lang));
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(camera_menu), GetLocStr(LocId::MENU_CAMERA, lang));
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(processing_menu), GetLocStr(LocId::MENU_PROCESSING, lang));
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(measurement_menu), GetLocStr(LocId::MENU_MEASUREMENT, lang));
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(help_menu), GetLocStr(LocId::MENU_HELP, lang));
    return menu;
}

void SyncMainMenu(HWND hwnd)
{
    HMENU menu = GetMenu(hwnd);
    if (!menu) {
        return;
    }
    CheckMenuItem(
        menu,
        kIdToggleFunctionPanel,
        MF_BYCOMMAND | (IsFunctionPanelVisible(hwnd) ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(
        menu,
        kIdDockFunctionPanelLeft,
        MF_BYCOMMAND | (IsFunctionPanelDockedLeft(hwnd) ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(
        menu,
        kIdDockFunctionPanelRight,
        MF_BYCOMMAND | (IsFunctionPanelDockedLeft(hwnd) ? MF_UNCHECKED : MF_CHECKED));

    const UILanguage lang = GetLanguageProperty(hwnd);
    CheckMenuItem(menu, kIdLanguageEnglish,
        MF_BYCOMMAND | (lang == UILanguage::English ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu, kIdLanguageChinese,
        MF_BYCOMMAND | (lang == UILanguage::Chinese ? MF_CHECKED : MF_UNCHECKED));

    DrawMenuBar(hwnd);
}

RECT GetPreviewRect(HWND hwnd)
{
    RECT rect = {};
    GetClientRect(hwnd, &rect);
    return WindowLayout::PreviewRect(
        rect,
        IsFunctionPanelVisible(hwnd),
        IsFunctionPanelDockedLeft(hwnd),
        FunctionPanelWidth(hwnd));
}

RECT GetSidePanelRect(HWND hwnd)
{
    RECT rect = {};
    GetClientRect(hwnd, &rect);
    return WindowLayout::SidePanelRect(
        rect,
        IsFunctionPanelVisible(hwnd),
        IsFunctionPanelDockedLeft(hwnd),
        FunctionPanelWidth(hwnd));
}

RECT GetStatusRect(HWND hwnd)
{
    RECT rect = {};
    GetClientRect(hwnd, &rect);
    return WindowLayout::StatusRect(rect);
}

void InvalidatePreview(HWND hwnd)
{
    RECT rect = GetPreviewRect(hwnd);
    InvalidateRect(hwnd, &rect, FALSE);
}

void InvalidateStatus(HWND hwnd)
{
    RECT rect = GetStatusRect(hwnd);
    InvalidateRect(hwnd, &rect, FALSE);
}

std::wstring Lowercase(std::wstring value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
    return value;
}

bool IsSupportedStitchImagePath(const std::filesystem::path& path)
{
    const std::wstring extension = Lowercase(path.extension().wstring());
    return extension == L".bmp" ||
        extension == L".jpg" ||
        extension == L".jpeg" ||
        extension == L".png" ||
        extension == L".tif" ||
        extension == L".tiff";
}

int NaturalCompare(const std::wstring& left, const std::wstring& right)
{
    std::size_t i = 0;
    std::size_t j = 0;
    while (i < left.size() && j < right.size()) {
        if (std::iswdigit(left[i]) && std::iswdigit(right[j])) {
            unsigned long long left_value = 0;
            unsigned long long right_value = 0;
            while (i < left.size() && std::iswdigit(left[i])) {
                left_value = left_value * 10ULL + static_cast<unsigned long long>(left[i] - L'0');
                ++i;
            }
            while (j < right.size() && std::iswdigit(right[j])) {
                right_value = right_value * 10ULL + static_cast<unsigned long long>(right[j] - L'0');
                ++j;
            }
            if (left_value != right_value) {
                return left_value < right_value ? -1 : 1;
            }
            continue;
        }

        const wchar_t left_char = static_cast<wchar_t>(std::towlower(left[i]));
        const wchar_t right_char = static_cast<wchar_t>(std::towlower(right[j]));
        if (left_char != right_char) {
            return left_char < right_char ? -1 : 1;
        }
        ++i;
        ++j;
    }
    if (left.size() == right.size()) {
        return 0;
    }
    return left.size() < right.size() ? -1 : 1;
}

bool NaturalPathLess(const std::filesystem::path& left, const std::filesystem::path& right)
{
    return NaturalCompare(left.filename().wstring(), right.filename().wstring()) < 0;
}
} // namespace

void LayoutControls(HWND hwnd, bool repaint_children = true);

class CameraPreviewApp {
    struct ReportTemplateDesignerState {
        CameraPreviewApp* app = nullptr;
        HWND title_label = nullptr;
        HWND title_edit = nullptr;
        HWND subtitle_label = nullptr;
        HWND subtitle_edit = nullptr;
        HWND page_layout_label = nullptr;
        HWND page_layout_standard = nullptr;
        HWND page_layout_wide = nullptr;
        HWND page_layout_compact = nullptr;
        HWND print_orientation_label = nullptr;
        HWND print_orientation_portrait = nullptr;
        HWND print_orientation_landscape = nullptr;
        HWND image_checkbox = nullptr;
        HWND image_size_label = nullptr;
        HWND image_size_original = nullptr;
        HWND image_size_fit = nullptr;
        HWND image_size_compact = nullptr;
        HWND image_caption_label = nullptr;
        HWND image_caption_edit = nullptr;
        HWND info_checkbox = nullptr;
        HWND notes_checkbox = nullptr;
        HWND summary_checkbox = nullptr;
        HWND table_checkbox = nullptr;
        HWND raw_values_checkbox = nullptr;
        HWND group_measurements_checkbox = nullptr;
        HWND measurement_precision_label = nullptr;
        HWND measurement_precision_auto = nullptr;
        HWND measurement_precision_two = nullptr;
        HWND measurement_precision_three = nullptr;
        HWND calibration_checkbox = nullptr;
        HWND processing_checkbox = nullptr;
        HWND footer_checkbox = nullptr;
        HWND footer_text_label = nullptr;
        HWND footer_text_edit = nullptr;
        HWND accent_label = nullptr;
        HWND accent_blue = nullptr;
        HWND accent_green = nullptr;
        HWND accent_gold = nullptr;
        HWND accent_magenta = nullptr;
        HWND info_label = nullptr;
        HWND info_edit = nullptr;
        HWND notes_label = nullptr;
        HWND notes_edit = nullptr;
        HWND section_label = nullptr;
        HWND section_list = nullptr;
        HWND section_heading_label = nullptr;
        HWND section_heading_edit = nullptr;
        HWND left_scroll_bar = nullptr;
        HWND preview = nullptr;
        HWND status = nullptr;
        HWND last_text_target = nullptr;
        struct PreviewSectionHitArea {
            ImageReportTemplateSection section = ImageReportTemplateSection::CurrentImage;
            RECT bounds{};
        };
        std::vector<PreviewSectionHitArea> preview_hit_areas;
        int left_scroll_offset = 0;
        int left_scroll_max = 0;
        int section_heading_anchor_top = 0;
        int section_heading_anchor_height = 0;
        std::vector<ImageReportTemplateSection> section_order;
        std::wstring current_image_heading;
        std::wstring report_information_heading;
        std::wstring notes_heading;
        std::wstring measurement_summary_heading;
        std::wstring measurement_table_heading;
        std::wstring image_details_heading;
        bool initialized = false;
        bool syncing_section_heading = false;
    };

public:
    explicit CameraPreviewApp(HWND hwnd) : hwnd_(hwnd)
    {
        SetFunctionPanelVisibleProperty(hwnd_, function_panel_visible_);
        SetFunctionPanelDockLeftProperty(hwnd_, function_panel_docked_left_);
        SetFunctionPanelWidthProperty(hwnd_, function_panel_width_);
    }
    ~CameraPreviewApp()
    {
        if (report_template_designer_ && IsWindow(report_template_designer_)) {
            DestroyWindow(report_template_designer_);
            report_template_designer_ = nullptr;
        }
        Stop();
        WaitForLiveStitchCaptureWorker();
        WaitForLiveStitchPreviewWorker();
        RequestProcessingCancel();
        WaitForProcessingWorker();
        ReleasePaintBuffer();
        RemoveFunctionPanelVisibleProperty(hwnd_);
        RemoveFunctionPanelDockLeftProperty(hwnd_);
        RemoveFunctionPanelWidthProperty(hwnd_);
    }

    void Start()
    {
        Stop();
        ClearLatestFrame();
        if (selected_camera_index_ < 0) {
            SetStatus(GetLocStr(LocId::STATUS_NO_CAMERA_SELECTED_SHORT, current_language_));
            return;
        }
        SetStatus(CameraControlStatusFormatter::FormatOpeningCamera(current_language_));
        running_ = true;
        worker_ = std::thread(&CameraPreviewApp::CaptureThread, this);
    }

    void Stop()
    {
        StopLiveStitchCapture(false);
        running_ = false;
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    bool RefreshCameraList(HWND combo)
    {
        Stop();
        ClearLatestFrame();

        if (combo) {
            SendMessageW(combo, CB_RESETCONTENT, 0, 0);
        }
        camera_count_ = 0;
        selected_camera_index_ = -1;
        camera_devices_.clear();

        CameraListRefreshActionResult result;
        std::vector<CameraDevice> devices;
        if (!camera_driver_.Load()) {
            result = CameraPanelActions::SdkUnavailable(camera_driver_.LastError());
        } else {
            const std::wstring sdk_telemetry = BuildSdkTelemetry();
            devices = camera_driver_.EnumerateDevices();
            result = CameraPanelActions::DevicesEnumerated(devices, sdk_telemetry);
        }

        camera_count_ = result.camera_count;
        selected_camera_index_ = result.selected_camera_index;
        camera_devices_ = std::move(devices);

        ApplyCameraDeviceListPresentation(combo, result.presentation);
        EnableWindow(GetDlgItem(hwnd_, kIdOpen), result.open_enabled ? TRUE : FALSE);
        EnableWindow(combo, result.combo_enabled ? TRUE : FALSE);
        SetPreviewTelemetry(result.preview_telemetry);
        SetStatus(result.status);
        return result.succeeded;
    }

    void UpdateSelectedCamera(HWND combo)
    {
        if (camera_count_ <= 0 || !combo) {
            selected_camera_index_ = -1;
            return;
        }

        const LRESULT selection = SendMessageW(combo, CB_GETCURSEL, 0, 0);
        const CameraSelectionActionResult result =
            CameraPanelActions::SelectDevice(
                static_cast<int>(camera_count_.load()),
                static_cast<int>(selection));
        selected_camera_index_ = result.selected_camera_index;
        if (result.has_status) {
            SetStatus(result.status);
        }
    }

    void ApplyExposure(HWND edit)
    {
        const CameraExposureParseResult parsed =
            CameraPanelActions::ParseExposureText(ReadEditText(edit, 64));
        if (!parsed.valid) {
            SetStatus(parsed.status);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(settings_mutex_);
            requested_exposure_ms_ = parsed.requested_exposure_ms;
        }

        if (camera_driver_.IsOpen() && camera_driver_.HasExposureControl()) {
            float min_value = 0.0f;
            float max_value = 0.0f;
            const bool has_range = camera_driver_.GetExposureRange(min_value, max_value);
            const float clamped =
                CameraPanelActions::ClampExposure(
                    parsed.requested_exposure_ms,
                    has_range,
                    min_value,
                    max_value);
            const bool applied = camera_driver_.SetExposure(clamped);
            SetStatus(CameraPanelActions::ExposureApplyStatus(applied, clamped));
        } else {
            SetStatus(CameraPanelActions::ExposurePendingStatus());
        }
    }

    void ApplyAutoExposure()
    {
        if (!camera_driver_.IsOpen()) {
            SetStatus(GetLocStr(LocId::STATUS_AEX_NEED_OPEN_CAMERA, current_language_));
            return;
        }
        if (!camera_driver_.HasAutoExposureControl() || !camera_driver_.ApplyAutoExposure()) {
            SetStatus(GetLocStr(LocId::STATUS_AEX_NOT_AVAILABLE, current_language_));
            return;
        }
        SetStatus(GetLocStr(LocId::STATUS_AEX_APPLIED, current_language_));
    }

    void ApplyGain(HWND edit)
    {
        double gain = 0.0;
        if (!ReadPositiveNumber(edit, gain)) {
            SetStatus(GetLocStr(LocId::STATUS_GAIN_NEED_POSITIVE, current_language_));
            return;
        }
        if (!camera_driver_.IsOpen()) {
            SetStatus(GetLocStr(LocId::STATUS_GAIN_NEED_OPEN_CAMERA, current_language_));
            return;
        }
        if (!camera_driver_.HasGainControl() || !camera_driver_.SetGain(static_cast<float>(gain))) {
            SetStatus(GetLocStr(LocId::STATUS_GAIN_NOT_AVAILABLE, current_language_));
            return;
        }
        SetStatus(GetLocStr(LocId::STATUS_GAIN_SET, current_language_));
    }

    void ApplyWhiteBalance()
    {
        if (!camera_driver_.IsOpen()) {
            SetStatus(GetLocStr(LocId::STATUS_WB_NEED_OPEN_CAMERA, current_language_));
            return;
        }
        if (!camera_driver_.HasWhiteBalanceControl() || !camera_driver_.ApplyWhiteBalance()) {
            SetStatus(GetLocStr(LocId::STATUS_WB_NOT_AVAILABLE, current_language_));
            return;
        }
        SetStatus(GetLocStr(LocId::STATUS_WB_APPLIED, current_language_));
    }

    void UpdatePseudoColor(HWND combo)
    {
        const LRESULT selection = combo ? SendMessageW(combo, CB_GETCURSEL, 0, 0) : 0;
        const PseudoColorSelectionResult result =
            PreviewDisplayActions::SelectPseudoColor(static_cast<int>(selection));
        pseudo_color_palette_ = result.palette;
        InvalidatePreviewFrameCache();
        InvalidatePreview(hwnd_);
        SetStatus(result.message);
    }

    void UpdateHistogramChannel(HWND combo)
    {
        const LRESULT sel = SendMessageW(combo, CB_GETCURSEL, 0, 0);
        if (sel >= 0 && sel < 4) {
            histogram_channel_ = static_cast<HistogramChannel>(sel);
        }
        InvalidatePreview(hwnd_);
    }

    void UpdateAdjustValueLabels(HWND hwnd)
    {
        WCHAR buf[32];
        const int b = image_adjust_.brightness;
        wsprintfW(buf, L"%+d", b);
        SetWindowTextW(GetDlgItem(hwnd, kIdHistogramBrightnessValue), buf);

        const int c = image_adjust_.contrast;
        wsprintfW(buf, c >= 0 ? L"+%d%%" : L"%d%%", c);
        SetWindowTextW(GetDlgItem(hwnd, kIdHistogramContrastValue), buf);

        swprintf_s(buf, L"%.1f", image_adjust_.gamma_tenths / 10.0);
        SetWindowTextW(GetDlgItem(hwnd, kIdHistogramGammaValue), buf);

        wsprintfW(buf, L"%d", image_adjust_.window_level);
        SetWindowTextW(GetDlgItem(hwnd, kIdHistogramWindowLevelValue), buf);

        wsprintfW(buf, L"%d", image_adjust_.window_width);
        SetWindowTextW(GetDlgItem(hwnd, kIdHistogramWindowWidthValue), buf);
    }

    void ResetImageAdjust(HWND hwnd)
    {
        image_adjust_ = ImageAdjustParams{};
        SendMessageW(GetDlgItem(hwnd, kIdHistogramBrightnessSlider), TBM_SETPOS, TRUE, 100);
        SendMessageW(GetDlgItem(hwnd, kIdHistogramContrastSlider), TBM_SETPOS, TRUE, 100);
        SendMessageW(GetDlgItem(hwnd, kIdHistogramGammaSlider), TBM_SETPOS, TRUE, 10);
        SendMessageW(GetDlgItem(hwnd, kIdHistogramWindowLevelSlider), TBM_SETPOS, TRUE, 128);
        SendMessageW(GetDlgItem(hwnd, kIdHistogramWindowWidthSlider), TBM_SETPOS, TRUE, 256);
        UpdateAdjustValueLabels(hwnd);
        InvalidatePreviewFrameCache();
        InvalidatePreview(hwnd_);
    }

    void OnHistogramSlider(HWND hwnd, LPARAM lp)
    {
        HWND slider = reinterpret_cast<HWND>(lp);
        if (!slider) {
            return;
        }
        const int pos = static_cast<int>(SendMessageW(slider, TBM_GETPOS, 0, 0));
        const int id = GetDlgCtrlID(slider);
        if (id == kIdStitchOverlapSlider) {
            UpdateStitchOverlapFromSlider(slider);
            return;
        }

        if (id == kIdHistogramBrightnessSlider) {
            image_adjust_.brightness = pos - 100;
        } else if (id == kIdHistogramContrastSlider) {
            image_adjust_.contrast = pos - 100;
        } else if (id == kIdHistogramGammaSlider) {
            image_adjust_.gamma_tenths = pos;
        } else if (id == kIdHistogramWindowLevelSlider) {
            image_adjust_.window_level = pos;
            // Clamp window-width to stay within 0..255
            ConstrainWindowWidth(hwnd);
        } else if (id == kIdHistogramWindowWidthSlider) {
            image_adjust_.window_width = pos;
        }

        UpdateAdjustValueLabels(hwnd);
        InvalidatePreviewFrameCache();
        InvalidatePreview(hwnd_);
    }

    void ConstrainWindowWidth(HWND hwnd)
    {
        const int wl = image_adjust_.window_level;
        const int max_ww = 2 * (std::min)(wl, 255 - wl);
        if (image_adjust_.window_width > max_ww) {
            image_adjust_.window_width = (std::max)(1, max_ww);
            SendMessageW(GetDlgItem(hwnd, kIdHistogramWindowWidthSlider), TBM_SETPOS, TRUE, image_adjust_.window_width);
        }
    }

    void SyncPanelCardButtons() const
    {
        const int card_ids[] = {
            kIdCameraPanelCard,
            kIdImagePanelCard,
            kIdFluorescencePanelCard,
            kIdProcessingPanelCard,
            kIdMeasurementPanelCard,
            kIdProjectPanelCard,
            kIdHistogramPanelCard
        };
        static const LocId card_loc_ids[] = {
            LocId::PANEL_CAMERA, LocId::PANEL_IMAGE, LocId::PANEL_FLUORESCENCE,
            LocId::PANEL_PROCESSING, LocId::PANEL_MEASUREMENT, LocId::PANEL_PROJECT,
            LocId::PANEL_HISTOGRAM
        };
        for (std::size_t i = 0; i < 7; ++i) {
            HWND button = GetDlgItem(hwnd_, card_ids[i]);
            if (!button) {
                continue;
            }
            SetWindowTextW(button, GetLocStr(card_loc_ids[i], current_language_));
            InvalidateRect(button, nullptr, TRUE);
        }
    }

    void ShowPanelCategory(int panel_category)
    {
        panel_category_ = WindowControlLayout::NormalizePanelCategory(panel_category);
        panel_scroll_offset_ = 0;
        SyncPanelCardButtons();
        static const LocId card_loc_ids[] = {
            LocId::PANEL_CAMERA, LocId::PANEL_IMAGE, LocId::PANEL_FLUORESCENCE,
            LocId::PANEL_PROCESSING, LocId::PANEL_MEASUREMENT, LocId::PANEL_PROJECT,
            LocId::PANEL_HISTOGRAM
        };
        SetStatus(std::wstring(GetLocStr(LocId::STATUS_FUNCTION_CARD, current_language_)) +
                  GetLocStr(card_loc_ids[panel_category_], current_language_) + L".");
    }

    int PanelCategory() const
    {
        return panel_category_;
    }

    bool FunctionPanelVisible() const
    {
        return function_panel_visible_;
    }

    bool FunctionPanelDockedLeft() const
    {
        return function_panel_docked_left_;
    }

    int FunctionPanelWidth() const
    {
        return function_panel_width_;
    }

    UILanguage CurrentLanguage() const { return current_language_; }

    void SyncFunctionPanelChrome() const
    {
        HWND toggle_button = GetDlgItem(hwnd_, kIdToggleFunctionPanel);
        if (toggle_button) {
            InvalidateRect(toggle_button, nullptr, FALSE);
        }
        HWND dock_button = GetDlgItem(hwnd_, kIdTogglePanelDock);
        if (dock_button) {
            InvalidateRect(dock_button, nullptr, FALSE);
        }
        SyncMainMenu(hwnd_);
    }

    void SetFunctionPanelVisible(bool visible)
    {
        if (function_panel_visible_ == visible) {
            SyncFunctionPanelChrome();
            return;
        }

        function_panel_visible_ = visible;
        SetFunctionPanelVisibleProperty(hwnd_, function_panel_visible_);
        if (!function_panel_visible_) {
            panel_scroll_offset_ = 0;
        }
        SyncFunctionPanelChrome();
        LayoutControls(hwnd_);
        InvalidateRect(hwnd_, nullptr, FALSE);
        SetStatus(GetLocStr(function_panel_visible_ ? LocId::STATUS_PANEL_SHOWN : LocId::STATUS_PANEL_HIDDEN, current_language_));
    }

    void ToggleFunctionPanel()
    {
        SetFunctionPanelVisible(!function_panel_visible_);
    }

    void ReloadUILanguage(UILanguage new_lang)
    {
        if (current_language_ == new_lang) return;
        current_language_ = new_lang;
        SetLanguageProperty(hwnd_, current_language_);

        // Rebuild menu bar with new language
        {
            HMENU old_menu = GetMenu(hwnd_);
            HMENU new_menu = CreateMainMenu(current_language_);
            SetMenu(hwnd_, new_menu);
            if (old_menu) DestroyMenu(old_menu);
        }

        // Update panel card headers (owner-draw buttons)
        SyncPanelCardButtons();

        // Update all side-panel control labels
        UpdatePanelControlTexts();

        // Sync chrome buttons and menu check marks
        SyncFunctionPanelChrome();
        LayoutControls(hwnd_);

        // Update histogram chart area labels
        UpdateAdjustValueLabels(hwnd_);

        InvalidateRect(hwnd_, nullptr, TRUE);
        SetStatus(GetLocStr(LocId::UI_READY, current_language_));
    }

    // ── About Dialog ──────────────────────────────────────────────────────

    struct AboutDialogData {
        const wchar_t* title;
        const wchar_t* desc;
        const wchar_t* version;
        const wchar_t* author;
        const wchar_t* ok_text;
    };

    static INT_PTR CALLBACK AboutDialogProc(HWND dlg, UINT msg, WPARAM wp, LPARAM)
    {
        if (msg == WM_INITDIALOG) {
            auto* data = reinterpret_cast<AboutDialogData*>(GetWindowLongPtrW(dlg, GWLP_USERDATA));
            if (!data) return TRUE;

            SetWindowTextW(dlg, data->title);

            // Set child control texts
            SetDlgItemTextW(dlg, 101, data->desc);
            SetDlgItemTextW(dlg, 102, data->version);
            SetDlgItemTextW(dlg, 103, data->author);
            SetDlgItemTextW(dlg, IDOK, data->ok_text);

            // Center the dialog over its parent
            HWND parent = GetParent(dlg);
            if (parent) {
                RECT pr, dr;
                GetWindowRect(parent, &pr);
                GetWindowRect(dlg, &dr);
                int dw = dr.right - dr.left;
                int dh = dr.bottom - dr.top;
                int px = pr.left + ((pr.right - pr.left) - dw) / 2;
                int py = pr.top + ((pr.bottom - pr.top) - dh) / 2;
                SetWindowPos(dlg, nullptr, px, py, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
            return TRUE;
        }
        if (msg == WM_COMMAND) {
            if (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL) {
                EndDialog(dlg, LOWORD(wp));
                return TRUE;
            }
        }
        return FALSE;
    }

    // Build a dialog template in dynamically allocated memory
    static std::vector<unsigned char> BuildAboutDialogTemplate()
    {
        // DLGTEMPLATE + menu/class (2 WORDs) + title array
        // + 3 static controls + 1 button = 4 DLGITEMTEMPLATEs
        constexpr int kItemCount = 4;
        constexpr int kDlgBaseUnitX = 4;
        constexpr int kDlgBaseUnitY = 8;

        // Dialog: 280 x 170 dlg units
        constexpr WORD dlgW = 280;
        constexpr WORD dlgH = 170;

        // Estimate total size
        size_t total = sizeof(DLGTEMPLATE) + sizeof(WORD) * 3; // menu, class, title
        total += kItemCount * (sizeof(DLGITEMTEMPLATE) + sizeof(WORD) * 3); // class, title, creation data
        total += 256; // generous padding for variable-length strings

        std::vector<unsigned char> buf(total, 0);
        unsigned char* p = buf.data();

        // DLGTEMPLATE header
        auto* dlg_tpl = reinterpret_cast<DLGTEMPLATE*>(p);
        dlg_tpl->style = WS_POPUP | WS_BORDER | WS_SYSMENU | WS_CAPTION | DS_SETFONT | DS_MODALFRAME;
        dlg_tpl->dwExtendedStyle = 0;
        dlg_tpl->cdit = kItemCount;
        dlg_tpl->x = 0;
        dlg_tpl->y = 0;
        dlg_tpl->cx = dlgW;
        dlg_tpl->cy = dlgH;
        p += sizeof(DLGTEMPLATE);

        // menu resource (0)
        *reinterpret_cast<WORD*>(p) = 0; p += sizeof(WORD);
        // class (0 = standard dialog)
        *reinterpret_cast<WORD*>(p) = 0; p += sizeof(WORD);
        // title (will be set at runtime via SetWindowText)
        *reinterpret_cast<WCHAR*>(p) = L'\0'; p += sizeof(WCHAR);
        // font size (8 pt)
        *reinterpret_cast<WORD*>(p) = 8; p += sizeof(WORD);

        // Helper lambda to align to DWORD
        auto align_dword = [](unsigned char*& ptr) {
            ptr = reinterpret_cast<unsigned char*>((reinterpret_cast<DWORD_PTR>(ptr) + 3) & ~3);
        };

        // ---- Item 1: Description static text (101) ----
        align_dword(p);
        auto* item1 = reinterpret_cast<DLGITEMTEMPLATE*>(p);
        item1->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
        item1->dwExtendedStyle = 0;
        item1->x = 12;
        item1->y = 10;
        item1->cx = dlgW - 24;
        item1->cy = 56;
        item1->id = 101;
        p += sizeof(DLGITEMTEMPLATE);
        *reinterpret_cast<WORD*>(p) = 0xFFFF; p += sizeof(WORD); // class: static
        *reinterpret_cast<WORD*>(p) = 0x0082; p += sizeof(WORD); // STATIC
        *reinterpret_cast<WCHAR*>(p) = L'\0'; p += sizeof(WCHAR); // text (set at runtime)
        *reinterpret_cast<WORD*>(p) = 0; p += sizeof(WORD); // no creation data

        // ---- Item 2: Version static text (102) ----
        align_dword(p);
        auto* item2 = reinterpret_cast<DLGITEMTEMPLATE*>(p);
        item2->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
        item2->dwExtendedStyle = 0;
        item2->x = 12;
        item2->y = 72;
        item2->cx = dlgW - 24;
        item2->cy = 14;
        item2->id = 102;
        p += sizeof(DLGITEMTEMPLATE);
        *reinterpret_cast<WORD*>(p) = 0xFFFF; p += sizeof(WORD);
        *reinterpret_cast<WORD*>(p) = 0x0082; p += sizeof(WORD);
        *reinterpret_cast<WCHAR*>(p) = L'\0'; p += sizeof(WCHAR);
        *reinterpret_cast<WORD*>(p) = 0; p += sizeof(WORD);

        // ---- Item 3: Author static text (103) ----
        align_dword(p);
        auto* item3 = reinterpret_cast<DLGITEMTEMPLATE*>(p);
        item3->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
        item3->dwExtendedStyle = 0;
        item3->x = 12;
        item3->y = 90;
        item3->cx = dlgW - 24;
        item3->cy = 14;
        item3->id = 103;
        p += sizeof(DLGITEMTEMPLATE);
        *reinterpret_cast<WORD*>(p) = 0xFFFF; p += sizeof(WORD);
        *reinterpret_cast<WORD*>(p) = 0x0082; p += sizeof(WORD);
        *reinterpret_cast<WCHAR*>(p) = L'\0'; p += sizeof(WCHAR);
        *reinterpret_cast<WORD*>(p) = 0; p += sizeof(WORD);

        // ---- Item 4: OK button (IDOK) ----
        align_dword(p);
        auto* item4 = reinterpret_cast<DLGITEMTEMPLATE*>(p);
        item4->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP;
        item4->dwExtendedStyle = 0;
        item4->x = (dlgW - 70) / 2;
        item4->y = dlgH - 36;
        item4->cx = 70;
        item4->cy = 24;
        item4->id = IDOK;
        p += sizeof(DLGITEMTEMPLATE);
        *reinterpret_cast<WORD*>(p) = 0xFFFF; p += sizeof(WORD);
        *reinterpret_cast<WORD*>(p) = 0x0080; p += sizeof(WORD); // BUTTON
        *reinterpret_cast<WCHAR*>(p) = L'\0'; p += sizeof(WCHAR);
        *reinterpret_cast<WORD*>(p) = 0; p += sizeof(WORD);

        buf.resize(p - buf.data());
        return buf;
    }

    void ShowAboutDialog()
    {
        static std::vector<unsigned char> template_buf = BuildAboutDialogTemplate();

        AboutDialogData data;
        data.title   = GetLocStr(LocId::ABOUT_TITLE, current_language_);
        data.desc    = GetLocStr(LocId::ABOUT_DESCRIPTION, current_language_);
        data.version = GetLocStr(LocId::ABOUT_VERSION, current_language_);
        data.author  = GetLocStr(LocId::ABOUT_AUTHOR, current_language_);
        data.ok_text = GetLocStr(LocId::ABOUT_OK, current_language_);

        DialogBoxIndirectParamW(GetModuleHandle(nullptr),
            reinterpret_cast<const DLGTEMPLATE*>(template_buf.data()),
            hwnd_, AboutDialogProc, reinterpret_cast<LPARAM>(&data));
    }

    void UpdatePanelControlTexts()
    {
        // Update histogram-specific labels
        struct {
            int ctrl_id;
            LocId loc_id;
        } extra_ctrls[] = {
            { kIdHistogramBrightnessLabel,  LocId::HIST_BRIGHTNESS },
            { kIdHistogramContrastLabel,    LocId::HIST_CONTRAST },
            { kIdHistogramGammaLabel,       LocId::HIST_GAMMA },
            { kIdHistogramWindowLevelLabel, LocId::HIST_WINDOW_LEVEL },
            { kIdHistogramWindowWidthLabel, LocId::HIST_WINDOW_WIDTH },
            { kIdHistogramResetAdjust,      LocId::HIST_RESET },
            { kIdHistogramChannelLabel,     LocId::HIST_CHANNEL },
        };
        for (const auto& c : extra_ctrls) {
            HWND ctrl = GetDlgItem(hwnd_, c.ctrl_id);
            if (ctrl) SetWindowTextW(ctrl, GetLocStr(c.loc_id, current_language_));
        }

    }

    void ApplyFunctionPanelDockedLeft(bool dock_left, bool announce)
    {
        if (function_panel_docked_left_ == dock_left) {
            SyncFunctionPanelChrome();
            return;
        }

        function_panel_docked_left_ = dock_left;
        SetFunctionPanelDockLeftProperty(hwnd_, function_panel_docked_left_);
        SyncFunctionPanelChrome();
        LayoutControls(hwnd_);
        InvalidateRect(hwnd_, nullptr, FALSE);
        if (announce) {
            SetStatus(function_panel_docked_left_ ? L"Function panel docked left." : L"Function panel docked right.");
        }
    }

    void SetFunctionPanelDockedLeft(bool dock_left)
    {
        ApplyFunctionPanelDockedLeft(dock_left, true);
    }

    void ToggleFunctionPanelDock()
    {
        SetFunctionPanelDockedLeft(!function_panel_docked_left_);
    }

    RECT FunctionPanelTitleRect() const
    {
        RECT title = GetSidePanelRect(hwnd_);
        if (!function_panel_visible_ || title.right <= title.left || title.bottom <= title.top) {
            return RECT{};
        }
        title.bottom = std::min(title.bottom, title.top + kPanelTitleHeight);
        return title;
    }

    bool PointInFunctionPanelTitle(POINT point) const
    {
        const RECT title = FunctionPanelTitleRect();
        return title.right > title.left && title.bottom > title.top && PtInRect(&title, point);
    }

    RECT FunctionPanelResizeGripRect() const
    {
        RECT grip = GetSidePanelRect(hwnd_);
        if (!function_panel_visible_ || grip.right <= grip.left || grip.bottom <= grip.top) {
            return RECT{};
        }
        if (function_panel_docked_left_) {
            grip.left = grip.right - kFunctionPanelResizeGripWidth / 2;
            grip.right += kFunctionPanelResizeGripWidth / 2;
        } else {
            grip.right = grip.left + kFunctionPanelResizeGripWidth / 2;
            grip.left -= kFunctionPanelResizeGripWidth / 2;
        }
        return grip;
    }

    bool PointInFunctionPanelResizeGrip(POINT point) const
    {
        const RECT grip = FunctionPanelResizeGripRect();
        return grip.right > grip.left && grip.bottom > grip.top && PtInRect(&grip, point);
    }

    bool ShouldShowFunctionPanelDockCursor(POINT point) const
    {
        return function_panel_resize_active_ ||
            function_panel_drag_active_ ||
            PointInFunctionPanelResizeGrip(point) ||
            PointInFunctionPanelTitle(point);
    }

    bool BeginFunctionPanelResize(POINT point)
    {
        if (!PointInFunctionPanelResizeGrip(point)) {
            return false;
        }

        function_panel_resize_active_ = true;
        SetCapture(hwnd_);
        SetStatus(GetLocStr(LocId::STATUS_PANEL_RESIZING, current_language_));
        return true;
    }

    bool ContinueFunctionPanelResize(POINT point)
    {
        if (!function_panel_resize_active_) {
            return false;
        }

        RECT client = {};
        GetClientRect(hwnd_, &client);
        const int requested_width = function_panel_docked_left_
            ? point.x - static_cast<int>(client.left)
            : static_cast<int>(client.right) - point.x;
        const int width = WindowLayout::ComputeSidePanelWidth(
            client,
            function_panel_visible_,
            requested_width);
        if (width > 0 && width != function_panel_width_) {
            function_panel_width_ = width;
            SetFunctionPanelWidthProperty(hwnd_, function_panel_width_);
            ClampPanelScroll();
            LayoutControls(hwnd_);
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return true;
    }

    bool EndFunctionPanelResize()
    {
        if (!function_panel_resize_active_) {
            return false;
        }

        function_panel_resize_active_ = false;
        if (GetCapture() == hwnd_) {
            ReleaseCapture();
        }
        SetStatus(GetLocStr(LocId::STATUS_PANEL_WIDTH_ADJUSTED, current_language_));
        return true;
    }

    bool BeginFunctionPanelDockDrag(POINT point)
    {
        if (!PointInFunctionPanelTitle(point)) {
            return false;
        }

        function_panel_drag_active_ = true;
        SetCapture(hwnd_);
        SetStatus(GetLocStr(LocId::STATUS_PANEL_DRAGGING, current_language_));
        return true;
    }

    bool ContinueFunctionPanelDockDrag(POINT point)
    {
        if (!function_panel_drag_active_) {
            return false;
        }

        RECT client = {};
        GetClientRect(hwnd_, &client);
        const int middle = client.left + (client.right - client.left) / 2;
        const bool target_left = point.x < middle;
        if (target_left != function_panel_docked_left_) {
            ApplyFunctionPanelDockedLeft(target_left, false);
            SetStatus(target_left ? L"Release to dock panel left." : L"Release to dock panel right.");
        }
        return true;
    }

    bool EndFunctionPanelDockDrag()
    {
        if (!function_panel_drag_active_) {
            return false;
        }

        function_panel_drag_active_ = false;
        if (GetCapture() == hwnd_) {
            ReleaseCapture();
        }
        SetStatus(function_panel_docked_left_ ? L"Function panel docked left." : L"Function panel docked right.");
        return true;
    }

    int PanelScrollOffset() const
    {
        return panel_scroll_offset_;
    }

    void ClampPanelScroll()
    {
        RECT client = {};
        GetClientRect(hwnd_, &client);
        if (!function_panel_visible_) {
            panel_scroll_offset_ = 0;
            return;
        }
        panel_scroll_offset_ = std::clamp(
            panel_scroll_offset_,
            0,
            WindowControlLayout::PanelScrollMax(
                client,
                panel_category_,
                function_panel_visible_,
                function_panel_docked_left_,
                function_panel_width_));
    }

    void SyncPanelScrollBar()
    {
        HWND scroll_bar = GetDlgItem(hwnd_, kIdPanelScrollBar);
        if (!scroll_bar) {
            return;
        }

        RECT client = {};
        GetClientRect(hwnd_, &client);
        const int max_scroll =
            WindowControlLayout::PanelScrollMax(
                client,
                panel_category_,
                function_panel_visible_,
                function_panel_docked_left_,
                function_panel_width_);
        if (max_scroll <= 0) {
            SCROLLINFO info = {};
            info.cbSize = sizeof(info);
            info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
            info.nMin = 0;
            info.nMax = 0;
            info.nPage = 1;
            info.nPos = 0;
            SetScrollInfo(scroll_bar, SB_CTL, &info, TRUE);
            return;
        }

        const int page_size = std::max(
            1,
            WindowControlLayout::PanelScrollPage(
                client,
                function_panel_visible_,
                function_panel_docked_left_,
                function_panel_width_));
        SCROLLINFO info = {};
        info.cbSize = sizeof(info);
        info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        info.nMin = 0;
        info.nMax = max_scroll + page_size - 1;
        info.nPage = static_cast<UINT>(page_size);
        info.nPos = panel_scroll_offset_;
        SetScrollInfo(scroll_bar, SB_CTL, &info, TRUE);
    }

    bool DrawPanelCategoryButton(const DRAWITEMSTRUCT& item) const
    {
        const int category = WindowControlLayout::PanelCategoryFromCardControl(static_cast<int>(item.CtlID));
        if (category < 0) {
            return false;
        }

        RECT rect = item.rcItem;
        const bool selected = category == panel_category_;
        FillSolidRect(item.hDC, rect, selected ? RGB(232, 235, 238) : RGB(205, 207, 209));

        RECT top_line = rect;
        top_line.bottom = top_line.top + 1;
        FillSolidRect(item.hDC, top_line, RGB(242, 243, 244));
        RECT bottom_line = rect;
        bottom_line.top = bottom_line.bottom - 2;
        FillSolidRect(item.hDC, bottom_line, RGB(150, 153, 156));

        RECT accent = rect;
        accent.left += 4;
        accent.top += 6;
        accent.right = accent.left + 18;
        accent.bottom -= 6;
        FillSolidRect(item.hDC, accent, selected ? RGB(0, 145, 210) : RGB(0, 160, 210));

        RECT text_rect = rect;
        text_rect.left += 30;
        text_rect.right -= 30;
        SetBkMode(item.hDC, TRANSPARENT);
        SetTextColor(item.hDC, RGB(0, 128, 210));
        HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HGDIOBJ old_font = SelectObject(item.hDC, font);
        const std::vector<std::wstring>& labels = WindowControlLayout::PanelCategoryLabels();
        const std::wstring& label = labels[static_cast<std::size_t>(category)];
        DrawTextW(
            item.hDC,
            label.c_str(),
            -1,
            &text_rect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (old_font) {
            SelectObject(item.hDC, old_font);
        }

        HPEN arrow_pen = CreatePen(PS_SOLID, 2, RGB(104, 108, 112));
        HGDIOBJ old_pen = SelectObject(item.hDC, arrow_pen);
        const int center_x = rect.right - 18;
        const int center_y = rect.top + (rect.bottom - rect.top) / 2;
        POINT points[3] = {};
        if (selected) {
            points[0] = POINT{center_x - 4, center_y - 2};
            points[1] = POINT{center_x, center_y + 3};
            points[2] = POINT{center_x + 4, center_y - 2};
        } else {
            points[0] = POINT{center_x - 2, center_y - 4};
            points[1] = POINT{center_x + 3, center_y};
            points[2] = POINT{center_x - 2, center_y + 4};
        }
        Polyline(item.hDC, points, 3);
        if (old_pen) {
            SelectObject(item.hDC, old_pen);
        }
        DeleteObject(arrow_pen);

        if ((item.itemState & ODS_FOCUS) != 0) {
            RECT focus = rect;
            InflateRect(&focus, -2, -2);
            DrawFocusRect(item.hDC, &focus);
        }
        return true;
    }

    // ── Toolbar icon button drawing ───────────────────────────────────────

    static bool IsToolbarButton(int control_id)
    {
        return control_id == kIdFitView ||
               control_id == kIdToggleFunctionPanel ||
               control_id == kIdTogglePanelDock;
    }

    // Draw a toolbar icon button with hover/active states and a colored icon
    bool DrawToolbarButton(const DRAWITEMSTRUCT& item)
    {
        if (!IsToolbarButton(static_cast<int>(item.CtlID))) {
            return false;
        }

        RECT rect = item.rcItem;
        const bool pressed = (item.itemState & ODS_SELECTED) != 0;
        const bool hover = (item.itemState & ODS_HOTLIGHT) != 0;
        const bool focused = (item.itemState & ODS_FOCUS) != 0;

        // Background
        COLORREF bg = RGB(40, 44, 52); // dark toolbar background
        if (pressed) {
            bg = RGB(24, 27, 32);
        } else if (hover) {
            bg = RGB(56, 60, 68);
        }
        FillSolidRect(item.hDC, rect, bg);

        // Accent color for icons
        constexpr COLORREF kAccent = RGB(97, 175, 239); // soft blue
        constexpr COLORREF kAccentDim = RGB(120, 120, 130);

        int cx = rect.left + (rect.right - rect.left) / 2;
        int cy = rect.top + (rect.bottom - rect.top) / 2;

        // Choose icon based on control ID
        HPEN pen = CreatePen(PS_SOLID, 2, pressed ? RGB(70, 140, 200) : (hover ? kAccent : kAccentDim));
        HGDIOBJ old_pen = SelectObject(item.hDC, pen);

        switch (item.CtlID) {
        case kIdFitView: {
            // Fit-to-view icon: rectangle with 4 corner arrows pointing inward
            const int s = 5; // half-size
            const int r = 9; // frame half-size
            // Outer frame
            Rectangle(item.hDC, cx - r, cy - r, cx + r + 1, cy + r + 1);
            // Four corner triangles (arrows pointing in)
            // Top-left
            MoveToEx(item.hDC, cx - r + 1, cy - r + 3, nullptr);
            LineTo(item.hDC, cx - r + 3, cy - r + 3);
            LineTo(item.hDC, cx - r + 3, cy - r + 1);
            MoveToEx(item.hDC, cx - r + 1, cy - r + 1, nullptr);
            LineTo(item.hDC, cx - r + s, cy - r + s);
            // Top-right
            MoveToEx(item.hDC, cx + r - 1, cy - r + 3, nullptr);
            LineTo(item.hDC, cx + r - 3, cy - r + 3);
            LineTo(item.hDC, cx + r - 3, cy - r + 1);
            MoveToEx(item.hDC, cx + r - 1, cy - r + 1, nullptr);
            LineTo(item.hDC, cx + r - s, cy - r + s);
            // Bottom-left
            MoveToEx(item.hDC, cx - r + 1, cy + r - 3, nullptr);
            LineTo(item.hDC, cx - r + 3, cy + r - 3);
            LineTo(item.hDC, cx - r + 3, cy + r - 1);
            MoveToEx(item.hDC, cx - r + 1, cy + r - 1, nullptr);
            LineTo(item.hDC, cx - r + s, cy + r - s);
            // Bottom-right
            MoveToEx(item.hDC, cx + r - 1, cy + r - 3, nullptr);
            LineTo(item.hDC, cx + r - 3, cy + r - 3);
            LineTo(item.hDC, cx + r - 3, cy + r - 1);
            MoveToEx(item.hDC, cx + r - 1, cy + r - 1, nullptr);
            LineTo(item.hDC, cx + r - s, cy + r - s);
            break;
        }
        case kIdToggleFunctionPanel: {
            // Side panel toggle icon: window with left panel
            const int r = 9;
            Rectangle(item.hDC, cx - r, cy - r, cx + r + 1, cy + r + 1);
            // Left panel
            RECT left_panel = {cx - r + 2, cy - r + 2, cx - 1, cy + r - 1};
            HBRUSH fill = CreateSolidBrush(pressed ? RGB(70, 140, 200) : (hover ? kAccent : kAccentDim));
            FillRect(item.hDC, &left_panel, fill);
            DeleteObject(fill);
            break;
        }
        case kIdTogglePanelDock: {
            // Dock toggle icon: arrow left/right in a frame
            const int r = 10;
            const int s = 5;
            // Frame
            Rectangle(item.hDC, cx - r, cy - r, cx + r + 1, cy + r + 1);
            if (function_panel_docked_left_) {
                // Arrow pointing right (dock right)
                MoveToEx(item.hDC, cx + 2, cy - s, nullptr);
                LineTo(item.hDC, cx + s + 1, cy);
                LineTo(item.hDC, cx + 2, cy + s);
            } else {
                // Arrow pointing left (dock left)
                MoveToEx(item.hDC, cx + 2, cy - s, nullptr);
                LineTo(item.hDC, cx - s + 1, cy);
                LineTo(item.hDC, cx + 2, cy + s);
            }
            break;
        }
        default:
            break;
        }

        SelectObject(item.hDC, old_pen);
        DeleteObject(pen);

        // Focus rect
        if (focused) {
            RECT focus_rect = rect;
            InflateRect(&focus_rect, -3, -3);
            SetTextColor(item.hDC, RGB(255, 255, 255));
            SetBkColor(item.hDC, RGB(40, 44, 52));
            DrawFocusRect(item.hDC, &focus_rect);
        }

        return true;
    }

    void InitializeObjectiveControls(HWND combo, HWND name_edit)
    {
        EnsureObjectiveCalibrationCount();
        RefreshObjectiveCombo(combo);
        SyncObjectiveNameEdit(name_edit);
        SyncCalibrationStatus();
    }

    void SelectObjective(HWND combo, HWND name_edit)
    {
        StoreActiveObjectiveCalibration();
        const LRESULT selection = combo ? SendMessageW(combo, CB_GETCURSEL, 0, 0) : selected_objective_index_;
        selected_objective_index_ = NormalizeObjectiveIndex(static_cast<int>(selection));
        EnsureObjectiveCalibrationCount();
        calibration_ = objective_calibrations_[static_cast<std::size_t>(selected_objective_index_)];
        measurement_interaction_.Clear();
        RefreshMeasurementList(GetDlgItem(hwnd_, kIdResultsList));
        SyncObjectiveNameEdit(name_edit);
        SyncCalibrationStatus();
        InvalidatePreviewFrameCache();
        InvalidatePreview(hwnd_);

        const std::wstring objective = ActiveObjectiveLabel();
        SetStatus(calibration_.IsCalibrated()
            ? objective + L" calibration loaded."
            : objective + L" has no saved calibration.");
    }

    void AddObjective(HWND combo, HWND name_edit)
    {
        const std::wstring objective = TextInputParser::Trim(ReadEditText(name_edit, 128));
        if (objective.empty()) {
            SetStatus(GetLocStr(LocId::OBJECTIVE_ENTER_NAME, current_language_));
            return;
        }
        if (ObjectiveLabelExists(objective)) {
            SetStatus(GetLocStr(LocId::OBJECTIVE_EXISTS, current_language_));
            return;
        }

        StoreActiveObjectiveCalibration();
        objective_labels_.push_back(objective);
        objective_calibrations_.push_back(CalibrationProfile::Uncalibrated());
        selected_objective_index_ = static_cast<int>(objective_labels_.size() - 1U);
        calibration_ = CalibrationProfile::Uncalibrated();
        measurement_interaction_.Clear();
        RefreshObjectiveCombo(combo);
        SyncObjectiveNameEdit(name_edit);
        RefreshMeasurementList(GetDlgItem(hwnd_, kIdResultsList));
        SyncCalibrationStatus();
        InvalidatePreviewFrameCache();
        InvalidatePreview(hwnd_);
        SetStatus(FormatLocStr(LocId::OBJECTIVE_ADDED, current_language_, {{L"{name}", objective}}));
    }

    void RenameSelectedObjective(HWND combo, HWND name_edit)
    {
        EnsureObjectiveCalibrationCount();
        const std::wstring objective = TextInputParser::Trim(ReadEditText(name_edit, 128));
        if (objective.empty()) {
            SetStatus(GetLocStr(LocId::OBJECTIVE_ENTER_NAME, current_language_));
            return;
        }
        if (ObjectiveLabelExists(objective, selected_objective_index_)) {
            SetStatus(GetLocStr(LocId::OBJECTIVE_EXISTS, current_language_));
            return;
        }

        objective_labels_[static_cast<std::size_t>(selected_objective_index_)] = objective;
        RefreshObjectiveCombo(combo);
        SyncObjectiveNameEdit(name_edit);
        SyncCalibrationStatus();
        SetStatus(FormatLocStr(LocId::OBJECTIVE_RENAMED, current_language_, {{L"{name}", objective}}));
    }

    void DeleteSelectedObjective(HWND combo, HWND name_edit)
    {
        EnsureObjectiveCalibrationCount();
        if (objective_labels_.size() <= 1U) {
            SetStatus(L"Keep at least one objective.");
            return;
        }

        const std::wstring removed = ActiveObjectiveLabel();
        const std::size_t selected = static_cast<std::size_t>(selected_objective_index_);
        objective_labels_.erase(objective_labels_.begin() + selected);
        objective_calibrations_.erase(objective_calibrations_.begin() + selected);
        selected_objective_index_ =
            std::min(selected_objective_index_, static_cast<int>(objective_labels_.size() - 1U));
        calibration_ = objective_calibrations_[static_cast<std::size_t>(selected_objective_index_)];
        measurement_interaction_.Clear();
        RefreshObjectiveCombo(combo);
        SyncObjectiveNameEdit(name_edit);
        RefreshMeasurementList(GetDlgItem(hwnd_, kIdResultsList));
        SyncCalibrationStatus();
        InvalidatePreviewFrameCache();
        InvalidatePreview(hwnd_);
        SetStatus(FormatLocStr(LocId::OBJECTIVE_DELETED, current_language_, {{L"{name}", removed}}));
    }

    void InitializeDyeCombo(HWND combo)
    {
        if (!combo) {
            return;
        }

        SendMessageW(combo, CB_RESETCONTENT, 0, 0);
        for (const std::wstring& text : FluorescenceDisplayActions::DyeLabels(dye_library_)) {
            SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        }
        if (!dye_library_.empty()) {
            SendMessageW(combo, CB_SETCURSEL, 0, 0);
        }
    }

    void SyncSelectedDyeControls(
        HWND combo,
        HWND name_edit,
        HWND excitation_edit,
        HWND emission_edit,
        HWND red_edit,
        HWND green_edit,
        HWND blue_edit)
    {
        const auto index = SelectedDyeIndex(combo);
        const DyeProfileFormValues values = index
            ? DyeProfileFormPresenter::FromDye(dye_library_[*index])
            : DyeProfileFormPresenter::Empty();
        SetWindowTextW(name_edit, values.name.c_str());
        SetWindowTextW(excitation_edit, values.excitation_nm.c_str());
        SetWindowTextW(emission_edit, values.emission_nm.c_str());
        SetWindowTextW(red_edit, values.red.c_str());
        SetWindowTextW(green_edit, values.green.c_str());
        SetWindowTextW(blue_edit, values.blue.c_str());
    }

    void SaveDyeProfile(
        HWND combo,
        HWND name_edit,
        HWND excitation_edit,
        HWND emission_edit,
        HWND red_edit,
        HWND green_edit,
        HWND blue_edit)
    {
        const DyeLibraryActionResult result = DyeLibraryActions::Save(dye_library_, DyeProfileInput{
            ReadEditText(name_edit, 128),
            ReadEditText(excitation_edit, 64),
            ReadEditText(emission_edit, 64),
            ReadEditText(red_edit, 32),
            ReadEditText(green_edit, 32),
            ReadEditText(blue_edit, 32)
        });
        if (!result.changed) {
            SetStatus(result.message);
            return;
        }

        InitializeDyeCombo(combo);
        if (combo && result.selected_index) {
            SendMessageW(combo, CB_SETCURSEL, *result.selected_index, 0);
        }
        SyncSelectedDyeControls(combo, name_edit, excitation_edit, emission_edit, red_edit, green_edit, blue_edit);
        SetStatus(result.message);
    }

    void DeleteSelectedDye(
        HWND combo,
        HWND name_edit,
        HWND excitation_edit,
        HWND emission_edit,
        HWND red_edit,
        HWND green_edit,
        HWND blue_edit)
    {
        const LRESULT selection = combo ? SendMessageW(combo, CB_GETCURSEL, 0, 0) : CB_ERR;
        const DyeLibraryActionResult result =
            DyeLibraryActions::DeleteSelected(dye_library_, static_cast<int>(selection));
        if (!result.changed) {
            SetStatus(result.message);
            return;
        }

        InitializeDyeCombo(combo);
        if (combo && result.selected_index) {
            SendMessageW(combo, CB_SETCURSEL, *result.selected_index, 0);
        }
        SyncSelectedDyeControls(combo, name_edit, excitation_edit, emission_edit, red_edit, green_edit, blue_edit);
        SetStatus(result.message);
    }

    void UpdateFusionPreview(HWND checkbox)
    {
        show_fusion_preview_ = checkbox && SendMessageW(checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
        InvalidatePreviewFrameCache();
        InvalidatePreview(hwnd_);
        SetStatus(show_fusion_preview_ ? L"Fusion preview: On." : L"Fusion preview: Off.");
    }

    void AddCurrentFrameAsChannel(HWND dye_combo, HWND channel_list, HWND fusion_checkbox)
    {
        ImageFrame frame = frame_buffer_.Snapshot();
        const DyeProfile dye = SelectedDye(dye_combo);
        const FluorescenceChannelListActionResult result =
            FluorescenceChannelListActions::AddCurrentFrame(
                fluorescence_channels_,
                std::move(frame),
                dye);
        if (!result.changed) {
            SetStatus(result.message);
            return;
        }

        show_fusion_preview_ = result.show_fusion_preview;
        InvalidatePreviewFrameCache();
        if (fusion_checkbox) {
            SendMessageW(fusion_checkbox, BM_SETCHECK, show_fusion_preview_ ? BST_CHECKED : BST_UNCHECKED, 0);
        }
        RefreshChannelList(channel_list);
        if (channel_list && result.selected_index) {
            SendMessageW(channel_list, LB_SETCURSEL, *result.selected_index, 0);
            SyncSelectedChannelControls(
                channel_list,
                GetDlgItem(hwnd_, kIdChannelVisible),
                GetDlgItem(hwnd_, kIdChannelBlackEdit),
                GetDlgItem(hwnd_, kIdChannelWhiteEdit));
        }
        InvalidatePreview(hwnd_);
        SetStatus(result.message);
    }

    void ClearFluorescenceChannels(HWND channel_list, HWND fusion_checkbox)
    {
        const FluorescenceChannelListActionResult result =
            FluorescenceChannelListActions::Clear(fluorescence_channels_);

        show_fusion_preview_ = result.show_fusion_preview;
        InvalidatePreviewFrameCache();
        if (fusion_checkbox) {
            SendMessageW(fusion_checkbox, BM_SETCHECK, show_fusion_preview_ ? BST_CHECKED : BST_UNCHECKED, 0);
        }
        RefreshChannelList(channel_list);
        SyncSelectedChannelControls(
            channel_list,
            GetDlgItem(hwnd_, kIdChannelVisible),
            GetDlgItem(hwnd_, kIdChannelBlackEdit),
            GetDlgItem(hwnd_, kIdChannelWhiteEdit));
        InvalidatePreview(hwnd_);
        SetStatus(result.message);
    }

    void SyncSelectedChannelControls(HWND list, HWND visible_checkbox, HWND black_edit, HWND white_edit)
    {
        const auto index = SelectedChannelIndex(list);
        const FluorescenceChannelFormValues values = index
            ? FluorescenceChannelFormPresenter::FromChannel(fluorescence_channels_[*index])
            : FluorescenceChannelFormPresenter::Empty();
        if (visible_checkbox) {
            SendMessageW(visible_checkbox, BM_SETCHECK, values.visible ? BST_CHECKED : BST_UNCHECKED, 0);
        }
        if (black_edit) {
            SetWindowTextW(black_edit, values.black_level.c_str());
        }
        if (white_edit) {
            SetWindowTextW(white_edit, values.white_level.c_str());
        }
    }

    void ApplySelectedChannelSettings(HWND list, HWND visible_checkbox, HWND black_edit, HWND white_edit)
    {
        int black_level = 0;
        int white_level = 255;
        if (!ReadByteValue(black_edit, black_level) || !ReadByteValue(white_edit, white_level)) {
            SetStatus(GetLocStr(LocId::STATUS_CHANNEL_RANGE, current_language_));
            return;
        }
        const bool visible = visible_checkbox && SendMessageW(visible_checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
        const int selection = list ? static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0)) : -1;
        const FluorescenceChannelUpdateResult result = FluorescenceChannelUpdater::Apply(
            fluorescence_channels_,
            selection,
            visible,
            black_level,
            white_level);
        if (!result.applied) {
            SetStatus(result.message);
            return;
        }

        RefreshChannelList(list);
        if (list && result.index) {
            SendMessageW(list, LB_SETCURSEL, *result.index, 0);
        }
        InvalidatePreviewFrameCache();
        InvalidatePreview(hwnd_);
        SetStatus(result.message);
    }

    void AddCurrentFrameAsStitchTile()
    {
        const auto add_tile_start = std::chrono::steady_clock::now();
        ImageFrame frame = frame_buffer_.Snapshot();
        const ProcessingQueueActionResult result =
            ProcessingQueueActions::AddStitchTileWithOverlap(
                stitch_tiles_,
                processing_frames_,
                std::move(frame),
                ReadEditText(GetDlgItem(hwnd_, kIdStitchSearchEdit), 32),
                stitch_overlap_percent_);
        const auto add_tile_elapsed = std::chrono::steady_clock::now() - add_tile_start;
        const long long add_tile_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(add_tile_elapsed).count();
        stitch_search_percent_ = result.stitch_search_percent;
        stitch_overlap_percent_ = result.stitch_overlap_percent;
        stitch_options_.overlap_percent = stitch_overlap_percent_;
        SyncStitchOverlapControls();
        if (result.preview_changed) {
            InvalidatePreviewFrameCache();
            InvalidatePreview(hwnd_);
        }
        if (result.changed) {
            InvalidateLiveStitchCaptureState();
            ClearLiveStitchPreviewTileCache();
            ClearLiveStitchReferenceTileCache();
            HWND stitch_tile_list = GetDlgItem(hwnd_, kIdStitchTileList);
            AppendStitchTileListItem(stitch_tile_list);
            if (stitch_tile_list && result.stitch_tile_count > 0) {
                SendMessageW(stitch_tile_list, LB_SETCURSEL, result.stitch_tile_count - 1, 0);
            }
            SetStitchSourceStatus(std::to_wstring(stitch_tiles_.size()) + L" image(s) | camera/current image");
        }
        SetStatus(FormatLocStr(LocId::TILE_ADDED, current_language_, {{L"{idx}", std::to_wstring(add_tile_ms)}}) + L" ms. " + result.message);
    }

    LiveStitchPreviewOptions CurrentLiveStitchPreviewOptions() const
    {
        LiveStitchPreviewOptions options;
        options.max_preview_edge = kLiveStitchPreviewMaxEdge;
        options.overlap_percent = stitch_overlap_percent_;
        options.blend_mode = stitch_options_.blend_mode;
        return options;
    }

    LiveStitchCaptureOptions CurrentLiveStitchCaptureOptions() const
    {
        LiveStitchCaptureOptions options;
        options.max_registration_edge = kLiveStitchRegistrationMaxEdge;
        options.min_movement_percent = kLiveStitchMinMovementPercent;
        options.min_overlap_percent = kLiveStitchMinOverlapPercent;
        options.search_percent =
            std::max(stitch_search_percent_, 100 - kLiveStitchMinOverlapPercent);
        options.fast_mode = true;
        options.reference_tile_count = kLiveStitchReferenceTileCount;
        return options;
    }

    void ClearLiveStitchPreviewTileCache()
    {
        live_stitch_preview_tiles_.clear();
        live_stitch_preview_tile_scale_ = 0;
    }

    void ClearLiveStitchReferenceTileCache()
    {
        live_stitch_reference_tiles_.clear();
    }

    static LiveStitchPreviewTile BuildSharedTileRef(
        const StitchTile& tile,
        std::shared_ptr<const ImageFrame> shared_frame = nullptr)
    {
        LiveStitchPreviewTile tile_ref;
        tile_ref.frame = shared_frame ? std::move(shared_frame) : std::make_shared<ImageFrame>(tile.frame);
        tile_ref.offset_x = tile.offset_x;
        tile_ref.offset_y = tile.offset_y;
        tile_ref.estimated_position = tile.estimated_position;
        return tile_ref;
    }

    static std::vector<StitchTile> BuildStitchTilesFromRefs(
        const std::vector<LiveStitchPreviewTile>& refs)
    {
        std::vector<StitchTile> tiles;
        tiles.reserve(refs.size());
        for (const LiveStitchPreviewTile& ref : refs) {
            if (!ref.frame || !ref.frame->IsValid()) {
                continue;
            }
            StitchTile tile;
            tile.frame = *ref.frame;
            tile.offset_x = ref.offset_x;
            tile.offset_y = ref.offset_y;
            tile.estimated_position = ref.estimated_position;
            tiles.push_back(std::move(tile));
        }
        return tiles;
    }

    void AppendLiveStitchReferenceTileCache(
        const StitchTile& tile,
        std::shared_ptr<const ImageFrame> shared_frame = nullptr)
    {
        if (!tile.frame.IsValid() && !shared_frame) {
            return;
        }
        live_stitch_reference_tiles_.push_back(BuildSharedTileRef(tile, std::move(shared_frame)));
        const std::size_t max_references =
            static_cast<std::size_t>(std::max(1, kLiveStitchReferenceTileCount));
        if (live_stitch_reference_tiles_.size() > max_references) {
            live_stitch_reference_tiles_.erase(
                live_stitch_reference_tiles_.begin(),
                live_stitch_reference_tiles_.begin() +
                    static_cast<std::ptrdiff_t>(live_stitch_reference_tiles_.size() - max_references));
        }
    }

    void RebuildLiveStitchReferenceTileCache()
    {
        ClearLiveStitchReferenceTileCache();
        const std::size_t reference_count =
            std::min(static_cast<std::size_t>(kLiveStitchReferenceTileCount), stitch_tiles_.size());
        const std::size_t first_index = stitch_tiles_.size() - reference_count;
        for (std::size_t index = first_index; index < stitch_tiles_.size(); ++index) {
            AppendLiveStitchReferenceTileCache(stitch_tiles_[index]);
        }
    }

    void AppendLiveStitchPreviewTileCache(
        const StitchTile& tile,
        std::shared_ptr<const ImageFrame> shared_frame = nullptr)
    {
        if (live_stitch_preview_tiles_.size() + 1U != stitch_tiles_.size()) {
            ClearLiveStitchPreviewTileCache();
            return;
        }
        if (live_stitch_preview_tile_scale_ <= 0) {
            live_stitch_preview_tile_scale_ =
                FastDownsampleScaleFor(tile.frame, kLiveStitchPreviewTileMaxEdge);
        }

        ImageFrame preview_frame =
            BuildFastDownsampledFrame(
                shared_frame ? *shared_frame : tile.frame,
                std::max(1, live_stitch_preview_tile_scale_));
        if (!preview_frame.IsValid()) {
            ClearLiveStitchPreviewTileCache();
            return;
        }

        LiveStitchPreviewTile preview_tile;
        preview_tile.frame = std::make_shared<ImageFrame>(std::move(preview_frame));
        preview_tile.offset_x =
            static_cast<int>(std::lround(
                static_cast<double>(tile.offset_x) / static_cast<double>(live_stitch_preview_tile_scale_)));
        preview_tile.offset_y =
            static_cast<int>(std::lround(
                static_cast<double>(tile.offset_y) / static_cast<double>(live_stitch_preview_tile_scale_)));
        preview_tile.estimated_position = tile.estimated_position;
        live_stitch_preview_tiles_.push_back(std::move(preview_tile));
    }

    void RebuildLiveStitchPreviewTileCache()
    {
        ClearLiveStitchPreviewTileCache();
        if (stitch_tiles_.empty()) {
            return;
        }

        int scale = 1;
        for (const StitchTile& tile : stitch_tiles_) {
            if (tile.frame.IsValid()) {
                scale = std::max(scale, FastDownsampleScaleFor(tile.frame, kLiveStitchPreviewTileMaxEdge));
            }
        }
        live_stitch_preview_tile_scale_ = std::max(1, scale);

        live_stitch_preview_tiles_.reserve(stitch_tiles_.size());
        for (const StitchTile& tile : stitch_tiles_) {
            if (!tile.frame.IsValid()) {
                continue;
            }

            ImageFrame preview_frame =
                BuildFastDownsampledFrame(tile.frame, live_stitch_preview_tile_scale_);
            if (!preview_frame.IsValid()) {
                ClearLiveStitchPreviewTileCache();
                return;
            }

            LiveStitchPreviewTile preview_tile;
            preview_tile.frame = std::make_shared<ImageFrame>(std::move(preview_frame));
            preview_tile.offset_x =
                static_cast<int>(std::lround(
                    static_cast<double>(tile.offset_x) / static_cast<double>(live_stitch_preview_tile_scale_)));
            preview_tile.offset_y =
                static_cast<int>(std::lround(
                    static_cast<double>(tile.offset_y) / static_cast<double>(live_stitch_preview_tile_scale_)));
            preview_tile.estimated_position = tile.estimated_position;
            live_stitch_preview_tiles_.push_back(std::move(preview_tile));
        }
    }

    std::vector<LiveStitchPreviewTile> BuildLiveStitchPreviewSnapshot() const
    {
        if (live_stitch_active_ &&
            !live_stitch_preview_tiles_.empty() &&
            live_stitch_preview_tiles_.size() == stitch_tiles_.size()) {
            return live_stitch_preview_tiles_;
        }
        if (live_stitch_active_) {
            return live_stitch_preview_tiles_;
        }

        std::vector<LiveStitchPreviewTile> snapshot;
        snapshot.reserve(stitch_tiles_.size());
        for (const StitchTile& tile : stitch_tiles_) {
            if (tile.frame.IsValid()) {
                snapshot.push_back(BuildSharedTileRef(tile));
            }
        }
        return snapshot;
    }

    std::vector<LiveStitchPreviewTile> BuildLiveStitchReferenceSnapshot()
    {
        if (live_stitch_reference_tiles_.empty() && !stitch_tiles_.empty()) {
            RebuildLiveStitchReferenceTileCache();
        }

        std::vector<LiveStitchPreviewTile> snapshot;
        const std::size_t reference_count =
            std::min(static_cast<std::size_t>(kLiveStitchReferenceTileCount), live_stitch_reference_tiles_.size());
        snapshot.reserve(reference_count);
        const std::size_t first_index = live_stitch_reference_tiles_.size() - reference_count;
        for (std::size_t index = first_index; index < live_stitch_reference_tiles_.size(); ++index) {
            snapshot.push_back(live_stitch_reference_tiles_[index]);
        }
        return snapshot;
    }

    void InvalidateLiveStitchPreviewState()
    {
        std::lock_guard<std::mutex> lock(live_stitch_preview_mutex_);
        ++live_stitch_preview_generation_;
        live_stitch_preview_min_generation_ = live_stitch_preview_generation_;
        live_stitch_preview_request_pending_ = false;
        live_stitch_preview_ready_ = false;
        live_stitch_preview_pending_tiles_.clear();
        live_stitch_preview_ready_image_ = ImageFrame();
        live_stitch_preview_ready_metadata_ = StitchResultMetadata();
    }

    void WaitForLiveStitchPreviewWorker()
    {
        InvalidateLiveStitchPreviewState();
        if (live_stitch_preview_worker_.joinable()) {
            live_stitch_preview_worker_.join();
        }
    }

    void RequestLiveStitchPreviewUpdate()
    {
        if (stitch_tiles_.empty()) {
            return;
        }

        std::vector<LiveStitchPreviewTile> tiles_snapshot = BuildLiveStitchPreviewSnapshot();
        const LiveStitchPreviewOptions options = CurrentLiveStitchPreviewOptions();
        bool start_worker = false;
        {
            std::lock_guard<std::mutex> lock(live_stitch_preview_mutex_);
            live_stitch_preview_pending_tiles_ = std::move(tiles_snapshot);
            live_stitch_preview_pending_options_ = options;
            live_stitch_preview_request_pending_ = true;
            ++live_stitch_preview_generation_;
            live_stitch_preview_pending_generation_ = live_stitch_preview_generation_;
            live_stitch_preview_min_generation_ = live_stitch_preview_generation_;
            if (!live_stitch_preview_worker_running_) {
                live_stitch_preview_worker_running_ = true;
                start_worker = true;
            }
        }

        if (start_worker) {
            if (live_stitch_preview_worker_.joinable()) {
                live_stitch_preview_worker_.join();
            }
            live_stitch_preview_worker_ = std::thread(&CameraPreviewApp::LiveStitchPreviewThread, this);
        }
    }

    void LiveStitchPreviewThread()
    {
        for (;;) {
            std::vector<LiveStitchPreviewTile> tiles;
            LiveStitchPreviewOptions options;
            unsigned long long generation = 0;
            {
                std::lock_guard<std::mutex> lock(live_stitch_preview_mutex_);
                if (!live_stitch_preview_request_pending_) {
                    live_stitch_preview_worker_running_ = false;
                    return;
                }
                tiles = std::move(live_stitch_preview_pending_tiles_);
                options = live_stitch_preview_pending_options_;
                generation = live_stitch_preview_pending_generation_;
                live_stitch_preview_request_pending_ = false;
            }

            LiveStitchPreviewResult preview =
                LiveStitchPreviewBuilder::Build(tiles, options);
            if (!preview.image.IsValid()) {
                continue;
            }

            bool should_post = false;
            {
                std::lock_guard<std::mutex> lock(live_stitch_preview_mutex_);
                if (generation >= live_stitch_preview_min_generation_) {
                    live_stitch_preview_ready_image_ = std::move(preview.image);
                    live_stitch_preview_ready_metadata_ = std::move(preview.metadata);
                    live_stitch_preview_ready_generation_ = generation;
                    live_stitch_preview_ready_ = true;
                    should_post = true;
                }
            }
            if (should_post) {
                PostMessageW(hwnd_, kMsgLiveStitchPreviewReady, 0, 0);
            }
        }
    }

    void ApplyLiveStitchPreviewResult()
    {
        ImageFrame image;
        StitchResultMetadata metadata;
        bool ready = false;
        {
            std::lock_guard<std::mutex> lock(live_stitch_preview_mutex_);
            ready =
                live_stitch_preview_ready_ &&
                live_stitch_preview_ready_generation_ >= live_stitch_preview_min_generation_;
            if (ready) {
                image = std::move(live_stitch_preview_ready_image_);
                metadata = std::move(live_stitch_preview_ready_metadata_);
                live_stitch_preview_ready_ = false;
            }
        }
        if (!ready || !image.IsValid()) {
            return;
        }

        if (processing_frames_.ShowStitchPreview(std::move(image), std::move(metadata))) {
            image_viewport_.Reset();
            InvalidatePreviewFrameCache();
            InvalidatePreview(hwnd_);
            const DWORD now = GetTickCount();
            if (live_stitch_active_ &&
                now - live_stitch_preview_status_tick_ >= kLiveStitchPreviewStatusMinIntervalMs) {
                live_stitch_preview_status_tick_ = now;
                SetLiveStitchStatus(L"Live stitch preview updated.");
            }
        }
    }

    void InvalidateLiveStitchCaptureState()
    {
        std::lock_guard<std::mutex> lock(live_stitch_capture_mutex_);
        ++live_stitch_capture_generation_;
        live_stitch_capture_min_generation_ = live_stitch_capture_generation_;
        live_stitch_capture_request_pending_ = false;
        live_stitch_capture_ready_ = false;
        live_stitch_capture_pending_request_ = LiveStitchCaptureRequest();
        live_stitch_capture_ready_result_ = LiveStitchCaptureResult();
    }

    void WaitForLiveStitchCaptureWorker()
    {
        InvalidateLiveStitchCaptureState();
        if (live_stitch_capture_worker_.joinable()) {
            live_stitch_capture_worker_.join();
        }
    }

    void RequestLiveStitchCaptureEvaluation(
        std::shared_ptr<const ImageFrame> frame,
        LiveStitchCaptureOptions options)
    {
        if (!frame || !frame->IsValid()) {
            return;
        }

        LiveStitchCaptureRequest request;
        request.reference_tiles = BuildLiveStitchReferenceSnapshot();
        request.frame = std::move(frame);
        request.options = options;
        request.sequence = request.frame->sequence;
        request.base_tile_count = stitch_tiles_.size();

        bool start_worker = false;
        {
            std::lock_guard<std::mutex> lock(live_stitch_capture_mutex_);
            ++live_stitch_capture_generation_;
            request.generation = live_stitch_capture_generation_;
            live_stitch_capture_min_generation_ = live_stitch_capture_generation_;
            live_stitch_capture_pending_request_ = std::move(request);
            live_stitch_capture_request_pending_ = true;
            if (!live_stitch_capture_worker_running_) {
                live_stitch_capture_worker_running_ = true;
                start_worker = true;
            }
        }

        if (start_worker) {
            if (live_stitch_capture_worker_.joinable()) {
                live_stitch_capture_worker_.join();
            }
            live_stitch_capture_worker_ = std::thread(&CameraPreviewApp::LiveStitchCaptureThread, this);
        }
    }

    void LiveStitchCaptureThread()
    {
        for (;;) {
            LiveStitchCaptureRequest request;
            {
                std::lock_guard<std::mutex> lock(live_stitch_capture_mutex_);
                if (!live_stitch_capture_request_pending_) {
                    live_stitch_capture_worker_running_ = false;
                    return;
                }
                request = std::move(live_stitch_capture_pending_request_);
                live_stitch_capture_pending_request_ = LiveStitchCaptureRequest();
                live_stitch_capture_request_pending_ = false;
            }

            const auto start = std::chrono::steady_clock::now();
            LiveStitchCaptureDecision decision;
            if (request.frame && request.frame->IsValid()) {
                std::vector<StitchTile> reference_tiles =
                    BuildStitchTilesFromRefs(request.reference_tiles);
                decision = LiveStitchCapturePlanner::Evaluate(
                    reference_tiles,
                    *request.frame,
                    request.options);
            } else {
                decision.message = L"Live stitch waiting for camera frame.";
            }

            LiveStitchCaptureResult result;
            result.decision = std::move(decision);
            result.frame = std::move(request.frame);
            result.generation = request.generation;
            result.sequence = request.sequence;
            result.base_tile_count = request.base_tile_count;
            result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();

            bool should_post = false;
            {
                std::lock_guard<std::mutex> lock(live_stitch_capture_mutex_);
                if (result.generation >= live_stitch_capture_min_generation_) {
                    live_stitch_capture_ready_result_ = std::move(result);
                    live_stitch_capture_ready_ = true;
                    should_post = true;
                }
            }
            if (should_post) {
                PostMessageW(hwnd_, kMsgLiveStitchCaptureReady, 0, 0);
            }
        }
    }

    void ApplyLiveStitchCaptureResult()
    {
        LiveStitchCaptureResult result;
        bool ready = false;
        {
            std::lock_guard<std::mutex> lock(live_stitch_capture_mutex_);
            ready =
                live_stitch_capture_ready_ &&
                live_stitch_capture_ready_result_.generation >= live_stitch_capture_min_generation_;
            if (ready) {
                result = std::move(live_stitch_capture_ready_result_);
                live_stitch_capture_ready_result_ = LiveStitchCaptureResult();
                live_stitch_capture_ready_ = false;
            }
        }
        if (!ready || !live_stitch_active_) {
            return;
        }

        const LiveStitchCaptureDecision& decision = result.decision;
        if (!decision.should_capture) {
            if (decision.match_missing) {
                live_stitch_out_of_range_candidate_count_ = 0;
                ++live_stitch_missing_match_count_;
                const bool confirmed_warning =
                    live_stitch_missing_match_count_ >= kLiveStitchMissingMatchWarningFrames;
                if (!confirmed_warning) {
                    live_stitch_out_of_range_warning_ = false;
                    SetStitchSourceStatus(
                        std::to_wstring(stitch_tiles_.size()) +
                        L" image(s) | live camera capture");
                    SetLiveStitchStatus(decision.message);
                    return;
                }

                const bool warning_changed = !live_stitch_out_of_range_warning_;
                const DWORD now = GetTickCount();
                if (warning_changed ||
                    now - live_stitch_last_warning_beep_tick_ >= kLiveStitchWarningBeepMinIntervalMs) {
                    MessageBeep(MB_ICONWARNING);
                    live_stitch_last_warning_beep_tick_ = now;
                }
                live_stitch_out_of_range_warning_ = true;
                SetStitchSourceStatus(
                    std::to_wstring(stitch_tiles_.size()) +
                    L" image(s) | WARNING: outside stitch range");
                SetLiveStitchStatus(
                    L"Live stitch warning: no overlap match for several frames. Move back toward the last tile.",
                    warning_changed);
                return;
            }

            if (decision.out_of_range_warning) {
                live_stitch_missing_match_count_ = 0;
                ++live_stitch_out_of_range_candidate_count_;
                const bool confirmed_warning =
                    live_stitch_out_of_range_candidate_count_ >= kLiveStitchOutOfRangeWarningFrames;
                if (!confirmed_warning) {
                    live_stitch_out_of_range_warning_ = false;
                    SetStitchSourceStatus(
                        std::to_wstring(stitch_tiles_.size()) +
                        L" image(s) | live camera capture");
                    SetLiveStitchStatus(
                        L"Live stitch registration uncertain. Keep at least " +
                        std::to_wstring(kLiveStitchMinOverlapPercent) +
                        L"% overlap.");
                    return;
                }

                const bool warning_changed = !live_stitch_out_of_range_warning_;
                const DWORD now = GetTickCount();
                if ((warning_changed ||
                        now - live_stitch_last_warning_beep_tick_ >= kLiveStitchWarningBeepMinIntervalMs)) {
                    MessageBeep(MB_ICONWARNING);
                    live_stitch_last_warning_beep_tick_ = now;
                }
                live_stitch_out_of_range_warning_ = true;
                SetStitchSourceStatus(
                    std::to_wstring(stitch_tiles_.size()) +
                    L" image(s) | WARNING: outside stitch range");
                if (!decision.message.empty()) {
                    SetLiveStitchStatus(decision.message, warning_changed);
                }
                return;
            }

            live_stitch_out_of_range_candidate_count_ = 0;
            live_stitch_missing_match_count_ = 0;
            live_stitch_out_of_range_warning_ = false;
            SetStitchSourceStatus(
                std::to_wstring(stitch_tiles_.size()) + L" image(s) | live camera capture");
            if (!decision.message.empty()) {
                SetLiveStitchStatus(decision.message);
            }
            return;
        }

        if (!result.frame || !result.frame->IsValid()) {
            SetLiveStitchStatus(L"Live stitch skipped: camera frame was no longer available.");
            return;
        }
        if (result.base_tile_count != stitch_tiles_.size()) {
            SetLiveStitchStatus(L"Live stitch skipped stale registration result.");
            return;
        }

        live_stitch_out_of_range_warning_ = false;
        live_stitch_out_of_range_candidate_count_ = 0;
        live_stitch_missing_match_count_ = 0;
        StitchTile tile;
        tile.frame = *result.frame;
        if (!stitch_tiles_.empty()) {
            tile.offset_x = decision.tile_offset_x;
            tile.offset_y = decision.tile_offset_y;
            tile.estimated_position = !decision.registration_valid;
        }
        stitch_tiles_.push_back(std::move(tile));
        AppendLiveStitchReferenceTileCache(stitch_tiles_.back(), result.frame);
        AppendLiveStitchPreviewTileCache(stitch_tiles_.back(), result.frame);
        ++live_stitch_capture_count_;
        processing_frames_.Clear();
        InvalidateLiveStitchCaptureState();

        HWND stitch_tile_list = GetDlgItem(hwnd_, kIdStitchTileList);
        AppendStitchTileListItem(stitch_tile_list);
        if (stitch_tile_list && !stitch_tiles_.empty()) {
            SendMessageW(stitch_tile_list, LB_SETCURSEL, stitch_tiles_.size() - 1U, 0);
        }
        RequestLiveStitchPreviewUpdate();
        SetStitchSourceStatus(
            std::to_wstring(stitch_tiles_.size()) + L" image(s) | live camera capture");

        std::wstring message =
            L"Live tile " + std::to_wstring(live_stitch_capture_count_) +
            L" captured. Registration " + std::to_wstring(result.elapsed_ms) +
            L" ms in background.";
        if (!decision.message.empty()) {
            message += L" " + decision.message;
        }
        message += L" Preview updating.";
        SetLiveStitchStatus(message, true);
    }

    void SyncLiveStitchControls()
    {
        if (HWND interval = GetDlgItem(hwnd_, kIdLiveStitchIntervalEdit)) {
            SetWindowTextW(interval, std::to_wstring(live_stitch_interval_ms_).c_str());
            EnableWindow(interval, live_stitch_active_ ? FALSE : TRUE);
        }
        EnableWindow(GetDlgItem(hwnd_, kIdStartLiveStitch), live_stitch_active_ ? FALSE : TRUE);
        EnableWindow(GetDlgItem(hwnd_, kIdStopLiveStitch), live_stitch_active_ ? TRUE : FALSE);
    }

    void StartLiveStitchCapture()
    {
        if (live_stitch_active_) {
            SetStatus(GetLocStr(LocId::STATUS_LS_ALREADY_RUNNING, current_language_));
            return;
        }
        if (!running_.load()) {
            SetStatus(GetLocStr(LocId::STATUS_LS_NEED_OPEN_CAMERA, current_language_));
            return;
        }
        if (!frame_buffer_.HasFrame()) {
            SetStatus(GetLocStr(LocId::STATUS_LS_WAITING_FRAME, current_language_));
            return;
        }

        int interval_ms = live_stitch_interval_ms_;
        if (!ReadIntegerRange(
                GetDlgItem(hwnd_, kIdLiveStitchIntervalEdit),
                kMinLiveStitchIntervalMs,
                kMaxLiveStitchIntervalMs,
                interval_ms,
                L"Live interval must be 250-10000 ms.")) {
            return;
        }

        InvalidateLiveStitchPreviewState();
        InvalidateLiveStitchCaptureState();
        ClearLiveStitchPreviewTileCache();
        ClearLiveStitchReferenceTileCache();
        RebuildLiveStitchReferenceTileCache();
        RebuildLiveStitchPreviewTileCache();
        live_stitch_active_ = true;
        live_stitch_interval_ms_ = interval_ms;
        live_stitch_last_evaluated_sequence_ = 0;
        live_stitch_capture_count_ = 0;
        live_stitch_out_of_range_warning_ = false;
        live_stitch_out_of_range_candidate_count_ = 0;
        live_stitch_missing_match_count_ = 0;
        live_stitch_last_warning_beep_tick_ = 0;
        live_stitch_tick_in_progress_ = false;
        live_stitch_last_status_tick_ = 0;
        live_stitch_preview_status_tick_ = 0;
        live_stitch_last_status_message_.clear();
        SyncLiveStitchControls();
        SetStatus(GetLocStr(LocId::STATUS_LS_STARTED, current_language_));
        SetTimer(hwnd_, kLiveStitchTimerId, static_cast<UINT>(live_stitch_interval_ms_), nullptr);
        CaptureLiveStitchTick();
    }

    void StopLiveStitchCapture(bool update_status = true)
    {
        if (!live_stitch_active_) {
            return;
        }
        KillTimer(hwnd_, kLiveStitchTimerId);
        live_stitch_active_ = false;
        live_stitch_tick_in_progress_ = false;
        live_stitch_out_of_range_candidate_count_ = 0;
        live_stitch_missing_match_count_ = 0;
        InvalidateLiveStitchCaptureState();
        SyncLiveStitchControls();
        if (update_status) {
            SetStatus(FormatLocStr(LocId::STATUS_LS_STOPPED, current_language_, {{L"{count}", std::to_wstring(stitch_tiles_.size())}}));
        }
    }

    void CaptureLiveStitchTick()
    {
        if (!live_stitch_active_) {
            return;
        }
        if (live_stitch_tick_in_progress_) {
            return;
        }

        live_stitch_tick_in_progress_ = true;
        struct TickGuard {
            bool& active;
            ~TickGuard() { active = false; }
        } guard{live_stitch_tick_in_progress_};

        if (processing_state_.IsRunning()) {
            SetLiveStitchStatus(L"Live stitch paused while processing is running.");
            return;
        }

        const std::shared_ptr<const ImageFrame> frame = frame_buffer_.SnapshotShared();
        if (!frame || !frame->IsValid()) {
            SetLiveStitchStatus(L"Live stitch waiting for camera frame.");
            return;
        }
        if (frame->sequence != 0 && frame->sequence == live_stitch_last_evaluated_sequence_) {
            return;
        }
        live_stitch_last_evaluated_sequence_ = frame->sequence;

        int overlap_percent = stitch_overlap_percent_;
        if (!ReadIntegerRange(
                GetDlgItem(hwnd_, kIdStitchSearchEdit),
                ProcessingParameterRules::MinStitchOverlapPercent(),
                ProcessingParameterRules::MaxStitchOverlapPercent(),
                overlap_percent,
                L"Estimated overlap must be 5-50 percent.")) {
            StopLiveStitchCapture(false);
            return;
        }
        if (overlap_percent != stitch_overlap_percent_) {
            stitch_overlap_percent_ = overlap_percent;
            stitch_search_percent_ = ProcessingParameterRules::SearchPercentFromOverlap(stitch_overlap_percent_);
            stitch_options_.overlap_percent = stitch_overlap_percent_;
            SyncStitchOverlapControls();
        }

        RequestLiveStitchCaptureEvaluation(frame, CurrentLiveStitchCaptureOptions());
        SetLiveStitchStatus(L"Live stitch registration running in background.");
    }

    void DeleteSelectedStitchTile()
    {
        HWND list = GetDlgItem(hwnd_, kIdStitchTileList);
        const LRESULT selection = list ? SendMessageW(list, LB_GETCURSEL, 0, 0) : LB_ERR;
        const StitchTileListActionResult result =
            StitchTileListActions::DeleteSelected(stitch_tiles_, static_cast<int>(selection));
        if (!result.changed) {
            SetStatus(result.message);
            return;
        }

        InvalidateLiveStitchPreviewState();
        InvalidateLiveStitchCaptureState();
        ClearLiveStitchPreviewTileCache();
        ClearLiveStitchReferenceTileCache();
        processing_frames_.Clear();
        RefreshStitchTileList(list);
        if (list && result.next_selection) {
            SendMessageW(list, LB_SETCURSEL, static_cast<WPARAM>(*result.next_selection), 0);
        }
        InvalidatePreviewFrameCache();
        InvalidatePreview(hwnd_);
        SetStitchSourceStatus(stitch_tiles_.empty()
            ? std::wstring(L"(no images selected)")
            : std::to_wstring(stitch_tiles_.size()) + L" image(s) | managed tiles");
        SetStatus(result.message);
    }
    void ClearStitchTiles()
    {
        StopLiveStitchCapture(false);
        const StitchTileListActionResult result = StitchTileListActions::Clear(stitch_tiles_);
        if (!result.changed) {
            SetStatus(result.message);
            return;
        }

        InvalidateLiveStitchPreviewState();
        InvalidateLiveStitchCaptureState();
        ClearLiveStitchPreviewTileCache();
        ClearLiveStitchReferenceTileCache();
        processing_frames_.Clear();
        RefreshStitchTileList(GetDlgItem(hwnd_, kIdStitchTileList));
        InvalidatePreviewFrameCache();
        InvalidatePreview(hwnd_);
        SetStitchSourceStatus(L"(no images selected)");
        SetStatus(result.message);
    }
    bool IsStitchOrbRegistrationEnabled() const
    {
        return IsDlgButtonChecked(hwnd_, kIdStitchOrbRegistration) == BST_CHECKED;
    }

    void SyncStitchRegistrationControl()
    {
        if (HWND checkbox = GetDlgItem(hwnd_, kIdStitchOrbRegistration)) {
            SendMessageW(
                checkbox,
                BM_SETCHECK,
                stitch_use_orb_registration_ ? BST_CHECKED : BST_UNCHECKED,
                0);
        }
    }

    static int ComboSelection(HWND combo, int fallback)
    {
        const LRESULT selection = combo ? SendMessageW(combo, CB_GETCURSEL, 0, 0) : CB_ERR;
        return selection == CB_ERR ? fallback : static_cast<int>(selection);
    }

    void SyncStitchOverlapControls()
    {
        stitch_overlap_percent_ = ProcessingParameterRules::ClampStitchOverlapPercent(stitch_overlap_percent_);
        stitch_search_percent_ = ProcessingParameterRules::SearchPercentFromOverlap(stitch_overlap_percent_);
        const std::wstring overlap_text = std::to_wstring(stitch_overlap_percent_);
        if (HWND edit = GetDlgItem(hwnd_, kIdStitchSearchEdit)) {
            SetWindowTextW(edit, overlap_text.c_str());
        }
        if (HWND value = GetDlgItem(hwnd_, kIdStitchOverlapValue)) {
            const std::wstring label = overlap_text + L"%";
            SetWindowTextW(value, label.c_str());
        }
        if (HWND slider = GetDlgItem(hwnd_, kIdStitchOverlapSlider)) {
            SendMessageW(slider, TBM_SETPOS, TRUE, stitch_overlap_percent_);
        }
    }

    void SyncStitchSourceStatusControl()
    {
        if (HWND label = GetDlgItem(hwnd_, kIdStitchSourceStatus)) {
            SetWindowTextW(label, stitch_source_status_.c_str());
        }
    }

    void SetStitchSourceStatus(std::wstring status)
    {
        if (stitch_source_status_ == status) {
            return;
        }
        stitch_source_status_ = std::move(status);
        SyncStitchSourceStatusControl();
    }

    void SetLiveStitchStatus(const std::wstring& message, bool force = false)
    {
        const DWORD now = GetTickCount();
        if (!force &&
            message == live_stitch_last_status_message_ &&
            now - live_stitch_last_status_tick_ < kLiveStitchStatusMinIntervalMs) {
            return;
        }

        live_stitch_last_status_tick_ = now;
        live_stitch_last_status_message_ = message;
        SetStatus(message);
    }

    void SyncStitchSettingsControls()
    {
        if (HWND mode = GetDlgItem(hwnd_, kIdStitchModeCombo)) {
            SendMessageW(mode, CB_SETCURSEL, stitch_options_.layout_mode == StitchLayoutMode::Linear ? 1 : 0, 0);
        }
        if (HWND rows = GetDlgItem(hwnd_, kIdStitchRowsEdit)) {
            SetWindowTextW(rows, std::to_wstring(stitch_options_.grid_rows).c_str());
        }
        if (HWND cols = GetDlgItem(hwnd_, kIdStitchColsEdit)) {
            SetWindowTextW(cols, std::to_wstring(stitch_options_.grid_cols).c_str());
        }
        if (HWND method = GetDlgItem(hwnd_, kIdStitchMethodCombo)) {
            int selection = 4;
            if (stitch_options_.registration_method == StitchRegistrationMethod::Phase) {
                selection = 0;
            } else if (stitch_options_.registration_method == StitchRegistrationMethod::Feature) {
                selection = 1;
            } else if (stitch_options_.registration_method == StitchRegistrationMethod::Auto) {
                selection = 2;
            } else if (stitch_options_.registration_method == StitchRegistrationMethod::Sift) {
                selection = 3;
            }
            SendMessageW(method, CB_SETCURSEL, selection, 0);
        }
        if (HWND transform = GetDlgItem(hwnd_, kIdStitchTransformCombo)) {
            int selection = 1;
            if (stitch_options_.transform_model == StitchTransformModel::Translation) {
                selection = 0;
            } else if (stitch_options_.transform_model == StitchTransformModel::Homography) {
                selection = 2;
            }
            SendMessageW(transform, CB_SETCURSEL, selection, 0);
        }
        if (HWND blend = GetDlgItem(hwnd_, kIdStitchBlendCombo)) {
            SendMessageW(blend, CB_SETCURSEL, stitch_options_.blend_mode == StitchBlendMode::None ? 1 : 0, 0);
        }
        SyncStitchOverlapControls();
        SyncStitchSourceStatusControl();
        SyncStitchRegistrationControl();
    }

    void InitializeStitchControls()
    {
        auto fill_combo = [](HWND combo, const std::vector<std::wstring>& values, int selection) {
            if (!combo) {
                return;
            }
            SendMessageW(combo, CB_RESETCONTENT, 0, 0);
            for (const std::wstring& value : values) {
                SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value.c_str()));
            }
            SendMessageW(combo, CB_SETCURSEL, selection, 0);
        };

        fill_combo(GetDlgItem(hwnd_, kIdStitchModeCombo), {L"grid", L"linear"}, 0);
        fill_combo(GetDlgItem(hwnd_, kIdStitchMethodCombo), {L"phase", L"feature", L"auto", L"sift", L"micro"}, 4);
        fill_combo(GetDlgItem(hwnd_, kIdStitchTransformCombo), {L"translation", L"affine", L"homography"}, 1);
        fill_combo(GetDlgItem(hwnd_, kIdStitchBlendCombo), {L"linear", L"none"}, 0);

        if (HWND slider = GetDlgItem(hwnd_, kIdStitchOverlapSlider)) {
            SendMessageW(
                slider,
                TBM_SETRANGE,
                TRUE,
                MAKELONG(
                    ProcessingParameterRules::MinStitchOverlapPercent(),
                    ProcessingParameterRules::MaxStitchOverlapPercent()));
            SendMessageW(slider, TBM_SETTICFREQ, 5, 0);
        }
        stitch_options_.overlap_percent = stitch_overlap_percent_;
        SyncStitchSettingsControls();
        SyncLiveStitchControls();
    }

    bool ReadIntegerRange(HWND edit, int min_value, int max_value, int& value, const std::wstring& message)
    {
        if (TextInputParser::TryParseIntegerRange(ReadEditText(edit, 32), min_value, max_value, value)) {
            return true;
        }
        SetStatus(message);
        return false;
    }

    std::optional<StitchProcessingOptions> ReadStitchProcessingOptions()
    {
        StitchProcessingOptions options = stitch_options_;
        options.layout_mode = ComboSelection(GetDlgItem(hwnd_, kIdStitchModeCombo), 0) == 1
            ? StitchLayoutMode::Linear
            : StitchLayoutMode::Grid;

        if (!ReadIntegerRange(
                GetDlgItem(hwnd_, kIdStitchRowsEdit),
                1,
                50,
                options.grid_rows,
                L"Rows must be 1-50.")) {
            return std::nullopt;
        }
        if (!ReadIntegerRange(
                GetDlgItem(hwnd_, kIdStitchColsEdit),
                1,
                50,
                options.grid_cols,
                L"Columns must be 1-50.")) {
            return std::nullopt;
        }
        if (!ReadIntegerRange(
                GetDlgItem(hwnd_, kIdStitchSearchEdit),
                ProcessingParameterRules::MinStitchOverlapPercent(),
                ProcessingParameterRules::MaxStitchOverlapPercent(),
                options.overlap_percent,
                L"Estimated overlap must be 5-50 percent.")) {
            return std::nullopt;
        }

        switch (ComboSelection(GetDlgItem(hwnd_, kIdStitchMethodCombo), 4)) {
        case 0:
            options.registration_method = StitchRegistrationMethod::Phase;
            break;
        case 1:
            options.registration_method = StitchRegistrationMethod::Feature;
            break;
        case 2:
            options.registration_method = StitchRegistrationMethod::Auto;
            break;
        case 3:
            options.registration_method = StitchRegistrationMethod::Sift;
            break;
        case 4:
        default:
            options.registration_method = StitchRegistrationMethod::Micro;
            break;
        }

        switch (ComboSelection(GetDlgItem(hwnd_, kIdStitchTransformCombo), 1)) {
        case 0:
            options.transform_model = StitchTransformModel::Translation;
            break;
        case 2:
            options.transform_model = StitchTransformModel::Homography;
            break;
        case 1:
        default:
            options.transform_model = StitchTransformModel::Affine;
            break;
        }
        options.blend_mode = ComboSelection(GetDlgItem(hwnd_, kIdStitchBlendCombo), 0) == 1
            ? StitchBlendMode::None
            : StitchBlendMode::Linear;
        return options;
    }

    void UpdateStitchOverlapFromSlider(HWND slider)
    {
        if (!slider) {
            return;
        }
        stitch_overlap_percent_ = static_cast<int>(SendMessageW(slider, TBM_GETPOS, 0, 0));
        stitch_options_.overlap_percent = stitch_overlap_percent_;
        SyncStitchOverlapControls();
    }

    void UpdateStitchSettingsFromControls()
    {
        const std::optional<StitchProcessingOptions> options = ReadStitchProcessingOptions();
        if (!options) {
            return;
        }
        stitch_options_ = *options;
        stitch_overlap_percent_ = stitch_options_.overlap_percent;
        stitch_use_orb_registration_ = stitch_options_.registration_method != StitchRegistrationMethod::Phase;
        SyncStitchOverlapControls();
        SyncStitchRegistrationControl();
        SetStatus(GetLocStr(LocId::STATUS_STITCH_SETTINGS_UPDATED, current_language_));
    }

    void UpdateStitchRegistrationMode()
    {
        UpdateStitchSettingsFromControls();
    }

    void ImportStitchImageFiles(const std::vector<std::wstring>& file_names, const std::wstring& source_label)
    {
        if (file_names.empty()) {
            SetStatus(GetLocStr(LocId::STATUS_NO_FILES_STITCH, current_language_));
            return;
        }

        std::vector<ImageFrame> frames;
        frames.reserve(file_names.size());
        std::size_t failed_count = 0;
        for (const std::wstring& file_name : file_names) {
            ImageFrame loaded;
            std::wstring error;
            if (ImageExporter::LoadRasterImage(std::filesystem::path(file_name), loaded, error)) {
                frames.push_back(std::move(loaded));
            } else {
                ++failed_count;
            }
        }
        if (frames.empty()) {
            SetStatus(GetLocStr(LocId::STATUS_NO_FILES_STITCH_TILES, current_language_));
            return;
        }

        const ProcessingIntegerInputResult overlap_input =
            ProcessingBuildInputActions::StitchOverlapForNextTile(
                !stitch_tiles_.empty() || frames.size() > 1U,
                ReadEditText(GetDlgItem(hwnd_, kIdStitchSearchEdit), 32),
                stitch_overlap_percent_);
        if (!overlap_input.accepted) {
            SetStatus(overlap_input.message);
            return;
        }
        stitch_overlap_percent_ = overlap_input.value;
        stitch_search_percent_ = ProcessingParameterRules::SearchPercentFromOverlap(stitch_overlap_percent_);
        stitch_options_.overlap_percent = stitch_overlap_percent_;
        SyncStitchOverlapControls();

        const StitchTileListActionResult result =
            StitchTileListActions::AddFrames(stitch_tiles_, std::move(frames), stitch_search_percent_);
        if (!result.changed) {
            SetStatus(result.message);
            return;
        }

        InvalidateLiveStitchPreviewState();
        InvalidateLiveStitchCaptureState();
        ClearLiveStitchPreviewTileCache();
        ClearLiveStitchReferenceTileCache();
        processing_frames_.Clear();
        RefreshStitchTileList(GetDlgItem(hwnd_, kIdStitchTileList));
        if (HWND stitch_tile_list = GetDlgItem(hwnd_, kIdStitchTileList)) {
            SendMessageW(stitch_tile_list, LB_SETCURSEL, result.tile_count - 1U, 0);
        }
        InvalidatePreviewFrameCache();
        InvalidatePreview(hwnd_);

        SetStitchSourceStatus(std::to_wstring(stitch_tiles_.size()) + L" image(s) | " + source_label);
        std::wstring message = result.message;
        if (failed_count > 0) {
            message += L" Skipped " + std::to_wstring(failed_count) + L" file(s).";
        }
        SetStatus(message);
    }

    void SelectStitchDirectory()
    {
        std::wstring directory;
        if (!FileDialog::OpenImageDirectory(hwnd_, directory)) {
            SetStatus(GetLocStr(LocId::STATUS_IMG_DIR_CANCELED, current_language_));
            return;
        }

        std::vector<std::filesystem::path> paths;
        std::error_code error;
        for (std::filesystem::directory_iterator it(directory, error), end; it != end && !error; it.increment(error)) {
            std::error_code file_error;
            if (!it->is_regular_file(file_error) || file_error) {
                continue;
            }
            const std::filesystem::path path = it->path();
            const std::wstring file_name = path.filename().wstring();
            if (!file_name.empty() && file_name.front() == L'_') {
                continue;
            }
            if (IsSupportedStitchImagePath(path)) {
                paths.push_back(path);
            }
        }
        if (error) {
            SetStatus(GetLocStr(LocId::STATUS_IMG_DIR_READ_FAILED, current_language_));
            return;
        }
        std::sort(paths.begin(), paths.end(), NaturalPathLess);

        std::vector<std::wstring> file_names;
        file_names.reserve(paths.size());
        for (const std::filesystem::path& path : paths) {
            file_names.push_back(path.wstring());
        }
        ImportStitchImageFiles(file_names, directory);
    }

    void SelectStitchFiles()
    {
        std::vector<std::wstring> file_names;
        if (!FileDialog::OpenImages(hwnd_, file_names)) {
            SetStatus(GetLocStr(LocId::STATUS_IMG_FILE_SEL_CANCELED, current_language_));
            return;
        }
        const std::filesystem::path first_path(file_names.empty() ? std::wstring() : file_names.front());
        ImportStitchImageFiles(
            file_names,
            first_path.empty() ? L"selected files" : first_path.parent_path().wstring());
    }

    void BuildStitchPreview()
    {
        StopLiveStitchCapture(false);
        InvalidateLiveStitchPreviewState();
        InvalidateLiveStitchCaptureState();
        ClearLiveStitchPreviewTileCache();
        ClearLiveStitchReferenceTileCache();
        const std::optional<StitchProcessingOptions> options = ReadStitchProcessingOptions();
        if (!options) {
            return;
        }
        stitch_options_ = *options;
        stitch_overlap_percent_ = stitch_options_.overlap_percent;
        stitch_search_percent_ = ProcessingParameterRules::SearchPercentFromOverlap(stitch_overlap_percent_);
        stitch_use_orb_registration_ = stitch_options_.registration_method != StitchRegistrationMethod::Phase;
        SyncStitchOverlapControls();
        SyncStitchRegistrationControl();

        ProcessingBuildActionResult result =
            ProcessingBuildInputActions::PrepareStitch(
                stitch_tiles_,
                ReadEditText(GetDlgItem(hwnd_, kIdStitchSearchEdit), 32),
                stitch_options_);
        if (!result.can_start) {
            SetStatus(result.message);
            return;
        }
        stitch_options_ = result.stitch_options;
        stitch_overlap_percent_ = stitch_options_.overlap_percent;
        stitch_search_percent_ = result.stitch_search_percent;
        stitch_use_orb_registration_ = result.stitch_use_orb_registration;
        StartStitchProcessing(
            std::move(result.stitch_tiles),
            result.stitch_options,
            true);
    }

    void SaveStitchResult()
    {
        if (processing_frames_.DisplaySource() != ProcessingResultDisplaySource::Stitch ||
            !processing_frames_.ProcessingResult().IsValid()) {
            SetStatus(L"No stitched result to save.");
            return;
        }

        std::wstring file_name;
        if (!FileDialog::SaveImage(hwnd_, file_name)) {
            SetStatus(GetLocStr(LocId::STATUS_STITCH_SAVE_CANCELED, current_language_));
            return;
        }

        const ExportActionResult result = ExportActions::SaveImage(
            std::filesystem::path(file_name),
            processing_frames_.ProcessingResult(),
            MeasurementCollection(),
            L"Stitch result",
            &calibration_);
        SetStatus(result.message + (result.saved ? L" Path: " + file_name : L""));
    }
    void AddCurrentFrameAsEdfFrame()
    {
        const ProcessingQueueActionResult result =
            ProcessingQueueActions::AddEdfFrame(edf_stack_, processing_frames_, frame_buffer_.Snapshot());
        if (result.preview_changed) {
            InvalidatePreviewFrameCache();
            InvalidatePreview(hwnd_);
        }
        SetStatus(result.message);
    }

    void BuildEdfPreview()
    {
        ProcessingBuildActionResult result =
            ProcessingBuildInputActions::PrepareEdf(
                edf_stack_,
                ReadEditText(GetDlgItem(hwnd_, kIdEdfRadiusEdit), 32));
        if (!result.can_start) {
            SetStatus(result.message);
            return;
        }
        edf_options_ = result.edf_options;
        StartEdfProcessing(std::move(result.edf_stack), result.edf_options, true);
    }

    void ShowEdfFocusMap()
    {
        const ProcessingPanelActionResult result =
            ProcessingPanelActions::ShowEdfFocusMap(processing_frames_);
        if (result.changed) {
            InvalidatePreviewFrameCache();
            InvalidatePreview(hwnd_);
        }
        SetStatus(result.message);
    }

    void ShowEdfCompositeFrame()
    {
        const ProcessingPanelActionResult result =
            ProcessingPanelActions::ShowEdfCompositeFrame(processing_frames_);
        if (result.changed) {
            InvalidatePreviewFrameCache();
            InvalidatePreview(hwnd_);
        }
        SetStatus(result.message);
    }

    void ClearProcessing()
    {
        StopLiveStitchCapture(false);
        InvalidateLiveStitchPreviewState();
        InvalidateLiveStitchCaptureState();
        ClearLiveStitchPreviewTileCache();
        ClearLiveStitchReferenceTileCache();
        const ProcessingPanelActionResult result = ProcessingPanelActions::Clear(
            processing_state_,
            stitch_tiles_,
            edf_stack_,
            processing_retry_,
            processing_frames_);
        RefreshStitchTileList(GetDlgItem(hwnd_, kIdStitchTileList));
        InvalidatePreviewFrameCache();
        InvalidatePreview(hwnd_);
        SetStatus(result.message);
    }

    void RetryProcessing()
    {
        ProcessingRetryActionResult result = ProcessingRetryActions::Prepare(processing_retry_);
        if (!result.can_start) {
            SetStatus(result.message);
            return;
        }

        if (result.kind == ProcessingJobKind::Stitch) {
            stitch_options_ = result.request.stitch_options;
            stitch_overlap_percent_ = stitch_options_.overlap_percent;
            stitch_search_percent_ = result.request.stitch_search_percent;
            stitch_use_orb_registration_ = result.request.stitch_use_orb_registration;
            SyncStitchSettingsControls();
            if (StartStitchProcessing(
                    std::move(result.request.stitch_tiles),
                    result.request.stitch_options,
                    false)) {
                SetStatus(result.message);
            }
            return;
        }

        if (result.kind == ProcessingJobKind::Edf) {
            if (StartEdfProcessing(
                    std::move(result.request.edf_stack),
                    result.request.edf_options,
                    false)) {
                SetStatus(result.message);
            }
        }
    }

    bool IsLiveCameraPreviewOverlayVisible() const
    {
        return running_.load() &&
            processing_frames_.IsProcessingResultVisible() &&
            processing_frames_.DisplaySource() == ProcessingResultDisplaySource::Stitch;
    }

    static bool HasDrawableBgrRows(const ImageFrame& frame)
    {
        if (!frame.IsValid() || frame.stride < frame.width * 3) {
            return false;
        }

        const std::size_t required =
            static_cast<std::size_t>(frame.height - 1) * static_cast<std::size_t>(frame.stride) +
            static_cast<std::size_t>(frame.width) * 3U;
        return frame.bgr.size() >= required;
    }

    static int FastDownsampleScaleFor(const ImageFrame& source, int max_edge)
    {
        if (!HasDrawableBgrRows(source)) {
            return 1;
        }

        const int capped_edge = std::max(64, max_edge);
        const int source_max_edge = std::max(source.width, source.height);
        return std::max(1, (source_max_edge + capped_edge - 1) / capped_edge);
    }

    static ImageFrame BuildFastDownsampledFrame(const ImageFrame& source, int scale)
    {
        if (!HasDrawableBgrRows(source)) {
            return {};
        }

        if (scale <= 1) {
            return source;
        }

        ImageFrame preview;
        preview.width = std::max(1, (source.width + scale - 1) / scale);
        preview.height = std::max(1, (source.height + scale - 1) / scale);
        preview.stride = (preview.width * 3 + 3) & ~3;
        preview.timestamp = source.timestamp;
        preview.sequence = source.sequence;
        preview.bgr.assign(
            static_cast<std::size_t>(preview.stride) * static_cast<std::size_t>(preview.height),
            0);

        for (int y = 0; y < preview.height; ++y) {
            const int source_y = std::min(source.height - 1, y * scale);
            const unsigned char* source_row =
                source.bgr.data() + static_cast<std::size_t>(source_y) * static_cast<std::size_t>(source.stride);
            unsigned char* preview_row =
                preview.bgr.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(preview.stride);
            for (int x = 0; x < preview.width; ++x) {
                const int source_x = std::min(source.width - 1, x * scale);
                const unsigned char* source_pixel = source_row + static_cast<std::size_t>(source_x) * 3U;
                unsigned char* preview_pixel = preview_row + static_cast<std::size_t>(x) * 3U;
                preview_pixel[0] = source_pixel[0];
                preview_pixel[1] = source_pixel[1];
                preview_pixel[2] = source_pixel[2];
            }
        }

        return preview;
    }

    static ImageFrame BuildLivePreviewOverlayFrame(const ImageFrame& source)
    {
        return BuildFastDownsampledFrame(
            source,
            FastDownsampleScaleFor(source, kLivePreviewOverlayMaxEdge));
    }

    RECT LiveCameraPreviewOverlayRect(const RECT& preview) const
    {
        constexpr int kMargin = 16;
        const int area_width = preview.right - preview.left;
        const int area_height = preview.bottom - preview.top;
        if (area_width <= kMargin * 2 + 80 || area_height <= kMargin * 2 + 80) {
            return RECT{0, 0, 0, 0};
        }

        int width = std::clamp(area_width / 4, 220, 360);
        int height = std::clamp(area_height / 4, 160, 260);
        width = std::min(width, area_width - kMargin * 2);
        height = std::min(height, area_height - kMargin * 2);
        return RECT{
            preview.right - kMargin - width,
            preview.top + kMargin,
            preview.right - kMargin,
            preview.top + kMargin + height};
    }

    static RECT FitFrameRect(const RECT& bounds, const ImageFrame& frame)
    {
        const int bounds_width = bounds.right - bounds.left;
        const int bounds_height = bounds.bottom - bounds.top;
        if (bounds_width <= 0 || bounds_height <= 0 || !frame.IsValid()) {
            return bounds;
        }

        const double scale = std::min(
            static_cast<double>(bounds_width) / static_cast<double>(std::max(1, frame.width)),
            static_cast<double>(bounds_height) / static_cast<double>(std::max(1, frame.height)));
        const int draw_width = std::max(1, static_cast<int>(std::lround(frame.width * scale)));
        const int draw_height = std::max(1, static_cast<int>(std::lround(frame.height * scale)));
        return RECT{
            bounds.left + (bounds_width - draw_width) / 2,
            bounds.top + (bounds_height - draw_height) / 2,
            bounds.left + (bounds_width - draw_width) / 2 + draw_width,
            bounds.top + (bounds_height - draw_height) / 2 + draw_height};
    }

    void DrawLiveCameraPreviewOverlay(HDC hdc, const RECT& preview)
    {
        if (!IsLiveCameraPreviewOverlayVisible()) {
            return;
        }

        const RECT panel = LiveCameraPreviewOverlayRect(preview);
        if (panel.right <= panel.left || panel.bottom <= panel.top) {
            return;
        }

        RECT shadow = panel;
        OffsetRect(&shadow, 3, 3);
        FillSolidRect(hdc, shadow, RGB(5, 8, 12));
        FillSolidRect(hdc, panel, RGB(24, 29, 36));

        HPEN border_pen = CreatePen(PS_SOLID, 1, RGB(94, 110, 130));
        HGDIOBJ old_pen = SelectObject(hdc, border_pen);
        HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, panel.left, panel.top, panel.right, panel.bottom);
        SelectObject(hdc, old_brush);
        SelectObject(hdc, old_pen);
        DeleteObject(border_pen);

        RECT header = panel;
        header.bottom = std::min(header.bottom, header.top + 24);
        FillSolidRect(hdc, header, RGB(37, 45, 55));
        RECT header_text = header;
        header_text.left += 8;
        header_text.right -= 8;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(226, 232, 240));
        DrawTextW(hdc, L"Live Preview", -1, &header_text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        RECT image_bounds = panel;
        image_bounds.left += 6;
        image_bounds.top = header.bottom + 6;
        image_bounds.right -= 6;
        image_bounds.bottom -= 6;
        FillSolidRect(hdc, image_bounds, RGB(12, 15, 20));

        const std::shared_ptr<const ImageFrame> live_frame = live_preview_overlay_buffer_.SnapshotShared();
        if (!live_frame || !HasDrawableBgrRows(*live_frame)) {
            SetTextColor(hdc, RGB(170, 180, 192));
            DrawTextW(hdc, L"No live frame", -1, &image_bounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return;
        }

        const RECT image_rect = FitFrameRect(image_bounds, *live_frame);
        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = live_frame->width;
        info.bmiHeader.biHeight = -live_frame->height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 24;
        info.bmiHeader.biCompression = BI_RGB;

        const int saved_dc = SaveDC(hdc);
        IntersectClipRect(hdc, image_bounds.left, image_bounds.top, image_bounds.right, image_bounds.bottom);
        SetStretchBltMode(hdc, COLORONCOLOR);
        StretchDIBits(
            hdc,
            image_rect.left,
            image_rect.top,
            image_rect.right - image_rect.left,
            image_rect.bottom - image_rect.top,
            0,
            0,
            live_frame->width,
            live_frame->height,
            live_frame->bgr.data(),
            &info,
            DIB_RGB_COLORS,
            SRCCOPY);
        if (saved_dc) {
            RestoreDC(hdc, saved_dc);
        }
    }

    void Paint(HDC hdc)
    {
        RECT client = {};
        GetClientRect(hwnd_, &client);

        RECT toolbar = client;
        toolbar.bottom = std::min(toolbar.bottom, static_cast<LONG>(WindowLayout::ToolbarHeight()));
        FillSolidRect(hdc, toolbar, RGB(40, 44, 52));

        RECT status = GetStatusRect(hwnd_);
        FillSolidRect(hdc, status, RGB(238, 241, 245));

        RECT preview = GetPreviewRect(hwnd_);
        FillSolidRect(hdc, preview, RGB(20, 23, 28));

        RECT side_panel = GetSidePanelRect(hwnd_);
        if (side_panel.right > side_panel.left) {
            FillSolidRect(hdc, side_panel, RGB(232, 235, 238));
            RECT panel_title = side_panel;
            panel_title.bottom = std::min(panel_title.bottom, panel_title.top + kPanelTitleHeight);
            FillSolidRect(hdc, panel_title, RGB(214, 224, 234));
            RECT panel_title_line = panel_title;
            panel_title_line.top = panel_title_line.bottom - 1;
            FillSolidRect(hdc, panel_title_line, RGB(166, 176, 186));

            RECT title_text = panel_title;
            title_text.left += 8;
            title_text.right -= 8;
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(22, 48, 72));
            static const LocId cat_loc_ids[] = {
                LocId::PANEL_CAMERA, LocId::PANEL_IMAGE, LocId::PANEL_FLUORESCENCE,
                LocId::PANEL_PROCESSING, LocId::PANEL_MEASUREMENT, LocId::PANEL_PROJECT,
                LocId::PANEL_HISTOGRAM
            };
            const std::wstring panel_label =
                std::wstring(GetLocStr(LocId::UI_FUNCTIONS, current_language_)) + L" - " +
                GetLocStr(cat_loc_ids[static_cast<std::size_t>(WindowControlLayout::NormalizePanelCategory(panel_category_))],
                          current_language_);
            DrawTextW(
                hdc,
                panel_label.c_str(),
                -1,
                &title_text,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            RECT separator = side_panel;
            if (function_panel_docked_left_) {
                separator.left = separator.right - 2;
            } else {
                separator.right = separator.left + 2;
            }
            FillSolidRect(hdc, separator, RGB(168, 174, 181));
        }

        const ImageFrame& raw_frame = CurrentPreviewFrame();
        bool use_adjusted = (panel_category_ == 6 && !image_adjust_.IsIdentity() && raw_frame.IsValid());
        if (use_adjusted) {
            ApplyAdjustments(raw_frame, adjusted_preview_, image_adjust_);
        }
        const ImageFrame& display_frame = use_adjusted ? adjusted_preview_ : raw_frame;
        if (display_frame.IsValid()) {
            image_viewport_.DrawFrame(hdc, preview, display_frame);
            overlay_renderer_.DrawMeasurementOverlay(
                hdc,
                preview,
                display_frame,
                image_viewport_,
                BuildMeasurementOverlayModel());
        } else {
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(210, 216, 224));
            DrawTextW(hdc, L"No frame", -1, &preview, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        DrawLiveCameraPreviewOverlay(hdc, preview);

        if (panel_category_ == 6) {
            constexpr int kHeaderH = 30;
            constexpr int kHeaderGap = 3;
            constexpr int kCardCount = 7;
            const RECT panel = GetSidePanelRect(hwnd_);
            const int content_start_y = panel.top + kPanelTitleHeight + 5
                                      + kCardCount * kHeaderH
                                      + (kCardCount - 1) * kHeaderGap
                                      + 8;
            // Controls layout offsets (matching case 6):
            //   channel:       24 + kControlHeight(28) + kControlGap(8) = 60
            //   b/c/g/wl rows: 22 + kControlHeight(28) + 2 = 52 (x4)
            //   ww row:        22 + kControlHeight(28) + kControlGap(8) = 58
            //   reset:         kControlHeight(28) + kControlGap(8) = 36
            //   total: 60 + 52*4 + 58 + 36 = 362
            constexpr int kHistoControlsEnd = 362;
            const int chart_y = content_start_y + kHistoControlsEnd + 8
                                - panel_scroll_offset_;
            const int chart_bot = (std::min)(chart_y + 140, static_cast<int>(panel.bottom - 28));
            RECT chart_rect = {panel.left + 12, chart_y, panel.right - 12, chart_bot};
            if (chart_rect.bottom > panel.top + 4 && chart_rect.right > chart_rect.left) {
                const ImageFrame& src = use_adjusted ? adjusted_preview_ : raw_frame;
                if (src.IsValid()) {
                    // --- Histogram cache ---
                    const unsigned long long seq = src.sequence;
                    if (histogram_cache_seq_ != seq ||
                        histogram_cache_ch_ != histogram_channel_ ||
                        histogram_cache_adjust_.brightness != image_adjust_.brightness ||
                        histogram_cache_adjust_.contrast != image_adjust_.contrast ||
                        histogram_cache_adjust_.gamma_tenths != image_adjust_.gamma_tenths ||
                        histogram_cache_adjust_.window_level != image_adjust_.window_level ||
                        histogram_cache_adjust_.window_width != image_adjust_.window_width) {
                        histogram_data_ = ComputeHistogram(src, histogram_channel_);
                        histogram_cache_seq_ = seq;
                        histogram_cache_ch_ = histogram_channel_;
                        histogram_cache_adjust_ = image_adjust_;
                    }

                    histo_renderer_.Draw(hdc, chart_rect, histogram_data_, histogram_channel_);

                    // --- Stats ---
                    int stats_y = chart_bot + 2;
                    int stats_h = 22;
                    int stats_bot = (std::min)(stats_y + stats_h, static_cast<int>(panel.bottom - 4));
                    RECT stats_rect = {panel.left + 12, stats_y, panel.right - 12, stats_bot};
                    if (stats_rect.bottom > stats_rect.top) {
                        FillSolidRect(hdc, stats_rect, RGB(32, 32, 36));
                        histo_renderer_.DrawStats(
                            hdc, stats_rect.left + 4, stats_rect.top,
                            stats_rect.right - stats_rect.left - 8,
                            histogram_data_.stats, histogram_channel_);
                    }
                }
            }

            if (!image_adjust_.IsIdentity()) {
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(255, 200, 60));
                HFONT hint_font = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                               CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                               DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
                HGDIOBJ old = SelectObject(hdc, hint_font);
                RECT hint_rect = {panel.left + 12, (std::min)(chart_bot + 24, static_cast<int>(panel.bottom - 20)),
                                  panel.right - 12, panel.bottom - 2};
                if (hint_rect.bottom > hint_rect.top) {
                    DrawTextW(hdc, L"Adjustments applied to preview", -1,
                              &hint_rect, DT_LEFT | DT_BOTTOM | DT_SINGLELINE);
                }
                SelectObject(hdc, old);
                DeleteObject(hint_font);
            }
        }

        std::wstring status_text;
        std::wstring telemetry_text;
        {
            std::lock_guard<std::mutex> lock(status_mutex_);
            status_text = status_;
            telemetry_text = preview_telemetry_;
        }

        status.left += 10;
        status.right -= 10;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(36, 42, 51));
        RECT status_left = status;
        RECT status_right = status;
        if (!telemetry_text.empty()) {
            status_right.left = std::max(status.left, status.right - 360);
            status_left.right = std::max(status.left, status_right.left - 12);
            SetTextColor(hdc, RGB(78, 88, 102));
            DrawTextW(hdc, telemetry_text.c_str(), -1, &status_right, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        SetTextColor(hdc, RGB(36, 42, 51));
        DrawTextW(hdc, status_text.c_str(), -1, &status_left, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    void PaintToWindow(HDC hdc, const RECT& dirty)
    {
        RECT client = {};
        GetClientRect(hwnd_, &client);
        const int width = client.right - client.left;
        const int height = client.bottom - client.top;
        const int dirty_width = dirty.right - dirty.left;
        const int dirty_height = dirty.bottom - dirty.top;
        if (width <= 0 || height <= 0 || dirty_width <= 0 || dirty_height <= 0) {
            return;
        }

        if (IsLiveCameraPreviewOverlayVisible()) {
            const RECT preview = GetPreviewRect(hwnd_);
            RECT overlay = LiveCameraPreviewOverlayRect(preview);
            if (overlay.right > overlay.left && overlay.bottom > overlay.top) {
                overlay.right += 4;
                overlay.bottom += 4;
                const bool dirty_inside_overlay =
                    dirty.left >= overlay.left &&
                    dirty.top >= overlay.top &&
                    dirty.right <= overlay.right &&
                    dirty.bottom <= overlay.bottom;
                if (dirty_inside_overlay && EnsurePaintBuffer(hdc, width, height)) {
                    const int saved_dc = SaveDC(paint_buffer_dc_);
                    if (saved_dc) {
                        IntersectClipRect(paint_buffer_dc_, dirty.left, dirty.top, dirty.right, dirty.bottom);
                    }
                    DrawLiveCameraPreviewOverlay(paint_buffer_dc_, preview);
                    if (saved_dc) {
                        RestoreDC(paint_buffer_dc_, saved_dc);
                    }
                    BitBlt(
                        hdc,
                        dirty.left,
                        dirty.top,
                        dirty_width,
                        dirty_height,
                        paint_buffer_dc_,
                        dirty.left,
                        dirty.top,
                        SRCCOPY);
                    return;
                }
            }
        }

        if (!EnsurePaintBuffer(hdc, width, height)) {
            Paint(hdc);
            return;
        }

        const int saved_dc = SaveDC(paint_buffer_dc_);
        if (saved_dc) {
            IntersectClipRect(paint_buffer_dc_, dirty.left, dirty.top, dirty.right, dirty.bottom);
        }
        Paint(paint_buffer_dc_);
        if (saved_dc) {
            RestoreDC(paint_buffer_dc_, saved_dc);
        }

        BitBlt(
            hdc,
            dirty.left,
            dirty.top,
            dirty_width,
            dirty_height,
            paint_buffer_dc_,
            dirty.left,
            dirty.top,
            SRCCOPY);
    }

    bool HandleMouseWheel(POINT screen_point, short wheel_delta)
    {
        POINT point = screen_point;
        ScreenToClient(hwnd_, &point);

        const RECT side_panel = GetSidePanelRect(hwnd_);
        if (PtInRect(&side_panel, point)) {
            return ScrollPanel(wheel_delta);
        }

        const RECT preview = GetPreviewRect(hwnd_);
        if (!PtInRect(&preview, point) || wheel_delta == 0) {
            return false;
        }

        const ImageFrame& frame = CurrentPreviewFrame();
        if (ViewportInteractionActions::ZoomAt(image_viewport_, preview, frame, point, wheel_delta)) {
            InvalidatePreview(hwnd_);
            SetStatus(ViewportInteractionActions::FormatZoomStatus(image_viewport_.Zoom()));
            return true;
        }
        return false;
    }

    bool ScrollPanel(short wheel_delta)
    {
        if (wheel_delta == 0) {
            return false;
        }

        const int direction = wheel_delta > 0 ? -1 : 1;
        return ScrollPanelTo(panel_scroll_offset_ + direction * 54);
    }

    bool HandlePanelScrollCommand(WORD scroll_request)
    {
        if (!function_panel_visible_) {
            return false;
        }

        RECT client = {};
        GetClientRect(hwnd_, &client);
        const int max_scroll =
            WindowControlLayout::PanelScrollMax(
                client,
                panel_category_,
                function_panel_visible_,
                function_panel_docked_left_,
                function_panel_width_);
        if (max_scroll <= 0) {
            return ScrollPanelTo(0);
        }

        const int page_size =
            std::max(
                54,
                WindowControlLayout::PanelScrollPage(
                    client,
                    function_panel_visible_,
                    function_panel_docked_left_,
                    function_panel_width_) - 24);
        int target_offset = panel_scroll_offset_;
        switch (scroll_request) {
        case SB_LINEUP:
            target_offset -= 32;
            break;
        case SB_LINEDOWN:
            target_offset += 32;
            break;
        case SB_PAGEUP:
            target_offset -= page_size;
            break;
        case SB_PAGEDOWN:
            target_offset += page_size;
            break;
        case SB_THUMBPOSITION:
        case SB_THUMBTRACK: {
            SCROLLINFO info = {};
            info.cbSize = sizeof(info);
            info.fMask = SIF_TRACKPOS;
            if (GetScrollInfo(GetDlgItem(hwnd_, kIdPanelScrollBar), SB_CTL, &info)) {
                target_offset = info.nTrackPos;
            }
            break;
        }
        case SB_TOP:
            target_offset = 0;
            break;
        case SB_BOTTOM:
            target_offset = max_scroll;
            break;
        default:
            return false;
        }

        return ScrollPanelTo(target_offset);
    }

    bool ScrollPanelTo(int target_offset)
    {
        if (!function_panel_visible_) {
            return false;
        }

        RECT client = {};
        GetClientRect(hwnd_, &client);
        const int max_scroll =
            WindowControlLayout::PanelScrollMax(
                client,
                panel_category_,
                function_panel_visible_,
                function_panel_docked_left_,
                function_panel_width_);
        const int old_offset = panel_scroll_offset_;
        panel_scroll_offset_ = std::clamp(target_offset, 0, max_scroll);
        if (panel_scroll_offset_ == old_offset) {
            SyncPanelScrollBar();
            return false;
        }

        const RECT side_panel = GetSidePanelRect(hwnd_);
        LayoutControls(hwnd_);
        RedrawWindow(
            hwnd_,
            &side_panel,
            nullptr,
            RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        return true;
    }

    void FitView()
    {
        const ImageFrame& frame = CurrentPreviewFrame();
        if (!frame.IsValid()) {
            SetStatus(GetLocStr(LocId::STATUS_IMAGE_NO_FRAME_FIT, current_language_));
            return;
        }

        image_viewport_.Reset();
        InvalidatePreview(hwnd_);
        SetStatus(L"Image fit to view. " + ViewportInteractionActions::FormatZoomStatus(image_viewport_.Zoom()));
    }

    bool BeginPan(POINT point)
    {
        const ImageFrame& frame = CurrentPreviewFrame();
        const ViewportPanBeginResult result =
            ViewportInteractionActions::BeginPan(
                viewport_pan_,
                GetPreviewRect(hwnd_),
                frame,
                point,
                edit_session_.IsActive());
        if (result.capture_mouse) {
            SetCapture(hwnd_);
        }
        return result.started;
    }

    bool ContinuePan(POINT point)
    {
        const ImageFrame& frame = CurrentPreviewFrame();
        const ViewportPanContinueResult result =
            ViewportInteractionActions::ContinuePan(
                viewport_pan_,
                image_viewport_,
                GetPreviewRect(hwnd_),
                frame,
                point);
        if (result.preview_changed) {
            InvalidatePreview(hwnd_);
        }
        return result.handled;
    }

    void EndPan()
    {
        if (!ViewportInteractionActions::EndPan(viewport_pan_)) {
            return;
        }
        if (GetCapture() == hwnd_) {
            ReleaseCapture();
        }
    }

    void BeginCalibration(HWND length_edit, HWND unit_combo)
    {
        double length = 0.0;
        const bool length_valid = ReadPositiveNumber(length_edit, length);
        const MeasurementUnit unit = SelectedCalibrationUnit(unit_combo);
        const MeasurementToolStartActionResult result =
            MeasurementToolStartActions::BeginCalibration(
                measurement_interaction_,
                CurrentMeasurementToolAvailability(MeasurementToolStartKind::Calibration),
                length_valid,
                length,
                unit);
        if (!result.started) {
            SetStatus(result.message);
            return;
        }

        pending_calibration_length_ = result.calibration_length;
        pending_calibration_unit_ = result.calibration_unit;
        SetStatus(result.message);
    }

    void ClearCalibration()
    {
        if (!calibration_.IsCalibrated()) {
            SyncCalibrationStatus();
            SetStatus(ActiveObjectiveLabel() + L" has no calibration to clear.");
            return;
        }

        calibration_ = CalibrationProfile::Uncalibrated();
        StoreActiveObjectiveCalibration();
        measurement_interaction_.Clear();
        RefreshMeasurementList(GetDlgItem(hwnd_, kIdResultsList));
        SyncCalibrationStatus();
        InvalidatePreviewFrameCache();
        InvalidatePreview(hwnd_);
        SetStatus(ActiveObjectiveLabel() + L" calibration cleared.");
    }

    void BeginLengthMeasurement()
    {
        const MeasurementToolStartActionResult result =
            MeasurementToolStartActions::BeginLength(
                measurement_interaction_,
                CurrentMeasurementToolAvailability(MeasurementToolStartKind::Measurement));
        SetStatus(result.message);
    }

    void BeginAngleMeasurement()
    {
        const MeasurementToolStartActionResult result =
            MeasurementToolStartActions::BeginAngle(
                measurement_interaction_,
                CurrentMeasurementToolAvailability(MeasurementToolStartKind::Measurement));
        SetStatus(result.message);
    }

    void BeginRectangleAreaMeasurement()
    {
        const MeasurementToolStartActionResult result =
            MeasurementToolStartActions::BeginRectangleArea(
                measurement_interaction_,
                CurrentMeasurementToolAvailability(MeasurementToolStartKind::Measurement));
        SetStatus(result.message);
    }

    void BeginPolygonAreaMeasurement()
    {
        const MeasurementToolStartActionResult result =
            MeasurementToolStartActions::BeginPolygonArea(
                measurement_interaction_,
                CurrentMeasurementToolAvailability(MeasurementToolStartKind::Measurement));
        SetStatus(result.message);
    }

    void FinishPolygonAreaMeasurement()
    {
        ApplyMeasurementInteractionActionResult(
            MeasurementInteractionActions::FinishPolygon(
                measurement_interaction_,
                measurements_,
                calibration_,
                pending_calibration_length_,
                pending_calibration_unit_,
                DisplayUnit()));
    }

    void ClearMeasurements(HWND list)
    {
        measurements_.Clear();
        measurement_interaction_.Clear();
        RefreshMeasurementList(list);
        InvalidatePreview(hwnd_);
        SetWindowTextW(GetDlgItem(hwnd_, kIdMeasurementNameEdit), L"");
        SetStatus(GetLocStr(LocId::STATUS_MEASUREMENTS_CLEARED, current_language_));
    }

    void DeleteSelectedMeasurement(HWND list)
    {
        const LRESULT selection = list ? SendMessageW(list, LB_GETCURSEL, 0, 0) : LB_ERR;
        const MeasurementListActionResult result =
            MeasurementListActions::DeleteSelected(measurements_, static_cast<int>(selection));
        if (!result.changed) {
            SetStatus(result.message);
            return;
        }

        RefreshMeasurementList(list);
        if (result.next_selection) {
            SendMessageW(list, LB_SETCURSEL, *result.next_selection, 0);
        }
        SyncSelectedMeasurementName(list, GetDlgItem(hwnd_, kIdMeasurementNameEdit));
        InvalidatePreview(hwnd_);
        SetStatus(result.message);
    }

    void SyncSelectedMeasurementName(HWND list, HWND edit)
    {
        if (!edit) {
            return;
        }

        const auto selection = SelectedMeasurement(list);
        if (!selection) {
            SetWindowTextW(edit, L"");
            return;
        }

        const std::wstring name = measurements_.Name(*selection);
        SetWindowTextW(edit, name.c_str());
    }

    void RenameSelectedMeasurement(HWND list, HWND edit)
    {
        wchar_t buffer[160] = {};
        if (edit) {
            GetWindowTextW(edit, buffer, static_cast<int>(sizeof(buffer) / sizeof(buffer[0])));
        }
        const LRESULT selected_index = list ? SendMessageW(list, LB_GETCURSEL, 0, 0) : LB_ERR;
        const MeasurementListActionResult result =
            MeasurementListActions::RenameSelected(measurements_, static_cast<int>(selected_index), buffer);
        if (!result.changed) {
            SetStatus(result.message);
            return;
        }

        RefreshMeasurementList(list);
        if (list && result.next_selection) {
            SendMessageW(list, LB_SETCURSEL, static_cast<int>(*result.next_selection), 0);
        }
        SetWindowTextW(edit, result.applied_name.c_str());
        InvalidatePreview(hwnd_);
        SetStatus(result.message);
    }

    bool BeginMeasurementEdit(POINT point, HWND list)
    {
        if (!measurement_interaction_.IsIdle() || viewport_pan_.active) {
            return false;
        }

        const ImageFrame& frame = CurrentPreviewFrame();
        if (!frame.IsValid()) {
            return false;
        }

        const RECT preview = GetPreviewRect(hwnd_);
        if (!PtInRect(&preview, point)) {
            return false;
        }

        const auto hit = MeasurementHitTester::FindEditableHandle(
            measurements_,
            image_viewport_,
            preview,
            frame,
            point);
        if (!hit) {
            return false;
        }

        edit_session_.Begin(*hit);
        if (list) {
            SendMessageW(list, LB_SETCURSEL, static_cast<WPARAM>(hit->flat_index), 0);
            SyncSelectedMeasurementName(list, GetDlgItem(hwnd_, kIdMeasurementNameEdit));
        }
        SetCapture(hwnd_);
        SetStatus(GetLocStr(LocId::STATUS_MEASUREMENT_DRAG_EDIT, current_language_));
        return true;
    }

    bool ContinueMeasurementEdit(POINT point)
    {
        if (!edit_session_.IsActive()) {
            return false;
        }

        const ImageFrame& frame = CurrentPreviewFrame();
        if (!frame.IsValid()) {
            return true;
        }

        const auto image_point = image_viewport_.ScreenToImage(GetPreviewRect(hwnd_), frame, point);
        if (!image_point) {
            return true;
        }

        const std::optional<MeasurementReference> edited_measurement =
            edit_session_.ApplyTo(measurements_, *image_point);
        if (!edited_measurement) {
            return true;
        }
        RefreshMeasurementList(GetDlgItem(hwnd_, kIdResultsList));
        SelectMeasurementInList(GetDlgItem(hwnd_, kIdResultsList), *edited_measurement);
        InvalidatePreview(hwnd_);
        return true;
    }

    void EndMeasurementEdit()
    {
        if (!edit_session_.IsActive()) {
            return;
        }

        edit_session_.Clear();
        if (GetCapture() == hwnd_) {
            ReleaseCapture();
        }
        SetStatus(GetLocStr(LocId::STATUS_MEASUREMENT_POINT_UPDATED, current_language_));
    }

    void ExportMeasurementsCsv()
    {
        if (measurements_.Empty()) {
            SetStatus(GetLocStr(LocId::STATUS_NO_MEASUREMENTS_EXPORT, current_language_));
            return;
        }

        std::wstring file_name;
        if (!FileDialog::SaveCsv(hwnd_, file_name)) {
            SetStatus(GetLocStr(LocId::STATUS_CSV_EXPORT_CANCELED, current_language_));
            return;
        }

        const ExportActionResult result = ExportActions::SaveMeasurementsCsv(
                std::filesystem::path(file_name),
                measurements_,
                calibration_,
                DisplayUnit(),
                ActiveObjectiveLabel());
        SetStatus(result.message);
    }

    void ExportImage()
    {
        const std::wstring display_mode = PreviewDisplayActions::PreviewModeLabel(
            frame_buffer_.Snapshot(),
            processing_frames_,
            fluorescence_channels_,
            show_fusion_preview_,
            pseudo_color_palette_);
        const ImageFrame export_frame = CurrentPreviewFrame();
        if (!export_frame.IsValid()) {
            SetStatus(GetLocStr(LocId::STATUS_NO_IMAGE_EXPORT, current_language_));
            return;
        }

        std::wstring file_name;
        if (!FileDialog::SaveImage(hwnd_, file_name)) {
            SetStatus(L"Image export canceled.");
            return;
        }

        const ExportActionResult result = ExportActions::SaveImage(
                std::filesystem::path(file_name),
                export_frame,
                measurements_,
                display_mode,
                &calibration_);
        SetStatus(result.message);
    }

    void OpenImage()
    {
        std::wstring file_name;
        if (!FileDialog::OpenImage(hwnd_, file_name)) {
            SetStatus(GetLocStr(LocId::STATUS_IMAGE_OPEN_CANCELED, current_language_));
            return;
        }

        OpenImageFile(file_name, false);
    }

    bool OpenImageFile(const std::wstring& file_name, bool dropped)
    {
        if (file_name.empty()) {
            SetStatus(L"No image file selected.");
            return false;
        }

        ImageFrame loaded;
        std::wstring error;
        if (!ImageExporter::LoadRasterImage(std::filesystem::path(file_name), loaded, error)) {
            SetStatus(error.empty() ? L"Failed to open image." : error);
            return false;
        }

        const std::wstring image_size =
            std::to_wstring(loaded.width) + L"x" + std::to_wstring(loaded.height);
        Stop();
        processing_state_.RequestCancel();
        processing_state_.InvalidateActiveJob();
        processing_frames_.Clear();
        frame_buffer_.Publish(std::move(loaded));
        InvalidatePreviewFrameCache();
        SetLatestFrameSource(L"Image file: " + file_name);
        image_viewport_.Reset();
        SetPreviewTelemetry(L"Loaded image | " + image_size);
        InvalidatePreview(hwnd_);
        SetStatus((dropped ? L"Dropped image opened: " : L"Image opened: ") + image_size + L".");
        return true;
    }

    void OpenDroppedFiles(const std::vector<std::wstring>& file_names)
    {
        if (file_names.empty()) {
            SetStatus(L"No dropped image files.");
            return;
        }
        if (file_names.size() == 1U) {
            OpenImageFile(file_names.front(), true);
            return;
        }

        std::vector<ImageFrame> frames;
        frames.reserve(file_names.size());
        std::size_t failed_count = 0;
        for (const std::wstring& file_name : file_names) {
            ImageFrame loaded;
            std::wstring error;
            if (ImageExporter::LoadRasterImage(std::filesystem::path(file_name), loaded, error)) {
                frames.push_back(std::move(loaded));
            } else {
                ++failed_count;
            }
        }
        if (frames.empty()) {
            SetStatus(L"No dropped image files could be added to stitch tiles.");
            return;
        }

        const ProcessingIntegerInputResult overlap_input =
            ProcessingBuildInputActions::StitchOverlapForNextTile(
                !stitch_tiles_.empty() || frames.size() > 1U,
                ReadEditText(GetDlgItem(hwnd_, kIdStitchSearchEdit), 32),
                stitch_overlap_percent_);
        if (!overlap_input.accepted) {
            SetStatus(overlap_input.message);
            return;
        }
        stitch_overlap_percent_ = overlap_input.value;
        stitch_search_percent_ = ProcessingParameterRules::SearchPercentFromOverlap(stitch_overlap_percent_);
        stitch_options_.overlap_percent = stitch_overlap_percent_;
        SyncStitchOverlapControls();

        const StitchTileListActionResult result =
            StitchTileListActions::AddFrames(stitch_tiles_, std::move(frames), stitch_search_percent_);
        if (!result.changed) {
            SetStatus(failed_count > 0
                ? L"No dropped image files could be added to stitch tiles."
                : result.message);
            return;
        }

        InvalidateLiveStitchPreviewState();
        InvalidateLiveStitchCaptureState();
        ClearLiveStitchPreviewTileCache();
        ClearLiveStitchReferenceTileCache();
        processing_frames_.Clear();
        RefreshStitchTileList(GetDlgItem(hwnd_, kIdStitchTileList));
        if (HWND stitch_tile_list = GetDlgItem(hwnd_, kIdStitchTileList)) {
            SendMessageW(stitch_tile_list, LB_SETCURSEL, result.tile_count - 1U, 0);
        }
        InvalidatePreviewFrameCache();
        InvalidatePreview(hwnd_);
        const std::filesystem::path first_path(file_names.front());
        SetStitchSourceStatus(
            std::to_wstring(stitch_tiles_.size()) + L" image(s) | " + first_path.parent_path().wstring());
        std::wstring message = result.message;
        if (failed_count > 0) {
            message += L" Skipped " + std::to_wstring(failed_count) + L" file(s).";
        }
        SetStatus(message);
    }

    void SaveDiagnosticsReport()
    {
        const ImageFrame& report_frame = CurrentPreviewFrame();
        if (!report_frame.IsValid()) {
            SetStatus(L"No image frame to report.");
            return;
        }

        std::wstring file_name;
        if (!FileDialog::SaveReport(hwnd_, file_name)) {
            SetStatus(GetLocStr(LocId::STATUS_REPORT_SAVE_CANCELED, current_language_));
            return;
        }

        const std::filesystem::path report_path =
            EnsureFileExtension(std::filesystem::path(file_name), L".html");
        const std::filesystem::path image_path = ReportImagePathFor(report_path);
        std::error_code directory_error;
        if (!image_path.parent_path().empty()) {
            std::filesystem::create_directories(image_path.parent_path(), directory_error);
        }
        if (directory_error) {
            SetStatus(L"Failed to create report image folder.");
            return;
        }

        const std::wstring preview_mode = PreviewDisplayActions::PreviewModeLabel(
            report_frame,
            processing_frames_,
            fluorescence_channels_,
            show_fusion_preview_,
            pseudo_color_palette_);
        const ExportActionResult image_result =
            ExportActions::SaveImage(image_path, report_frame, measurements_, preview_mode, &calibration_);
        if (!image_result.saved) {
            SetStatus(L"Report image failed: " + image_result.message);
            return;
        }

        const std::wstring report = BuildImageReport(image_path.filename().wstring(), report_frame);
        const ExportActionResult report_result =
            ExportActions::SaveReportHtml(report_path, report);
        if (!report_result.saved) {
            SetStatus(report_result.message);
            return;
        }

        SetStatus(
            L"Report saved: " + AbsolutePathText(report_path) +
            L". Image: " + AbsolutePathText(image_path) + L".");
    }

    void LoadReportTemplate()
    {
        std::wstring file_name;
        if (!FileDialog::OpenText(hwnd_, file_name)) {
            SetStatus(L"Report template load canceled.");
            return;
        }

        std::wstring text;
        std::wstring error;
        if (!ReadTextFile(std::filesystem::path(file_name), text, error)) {
            SetStatus(error.empty() ? L"Failed to load report template." : error);
            return;
        }
        if (text.empty()) {
            SetStatus(L"Report template is empty.");
            return;
        }

        report_template_text_ = std::move(text);
        report_template_path_ = file_name;
        ImageReportTemplateOptions loaded_visual_options;
        const bool loaded_visual_template =
            DiagnosticReportActions::TryParseImageReportTemplateOptions(
                report_template_text_,
                loaded_visual_options);
        visual_report_template_options_ = loaded_visual_template
            ? loaded_visual_options
            : ImageReportTemplateOptions{};
        SyncReportTemplateStatus();
        SetStatus(
            L"Report template loaded: " +
            std::filesystem::path(file_name).filename().wstring() +
            (loaded_visual_template ? L" (visual settings restored)." : L"."));
    }

    void ClearReportTemplate()
    {
        report_template_text_.clear();
        report_template_path_.clear();
        visual_report_template_options_ = ImageReportTemplateOptions{};
        SyncReportTemplateStatus();
        SetStatus(GetLocStr(LocId::STATUS_REPORT_TEMPLATE_CLEARED, current_language_));
    }

    void ShowReportTemplateDesigner()
    {
        if (report_template_designer_ && IsWindow(report_template_designer_)) {
            ShowWindow(report_template_designer_, SW_SHOWNORMAL);
            SetForegroundWindow(report_template_designer_);
            return;
        }

        RegisterReportTemplateDesignerClass();
        auto* state = new ReportTemplateDesignerState;
        state->app = this;

        RECT parent_rect = {};
        GetWindowRect(hwnd_, &parent_rect);
        RECT work_area = {};
        if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0)) {
            work_area = parent_rect;
        }
        const int work_width = static_cast<int>(work_area.right - work_area.left);
        const int work_height = static_cast<int>(work_area.bottom - work_area.top);
        const int max_width = std::max(kReportTemplateDesignerMinWidth, work_width - 40);
        const int max_height = std::max(kReportTemplateDesignerMinHeight, work_height - 40);
        const int width = std::clamp(
            kReportTemplateDesignerDefaultWidth,
            kReportTemplateDesignerMinWidth,
            max_width);
        const int height = std::clamp(
            kReportTemplateDesignerDefaultHeight,
            kReportTemplateDesignerMinHeight,
            max_height);
        const int parent_width = static_cast<int>(parent_rect.right - parent_rect.left);
        const int parent_height = static_cast<int>(parent_rect.bottom - parent_rect.top);
        auto clamp_position = [](int value, int low, int high) {
            return low <= high ? std::clamp(value, low, high) : low;
        };
        const int centered_x = static_cast<int>(parent_rect.left) + (parent_width - width) / 2;
        const int centered_y = static_cast<int>(parent_rect.top) + (parent_height - height) / 2;
        const int x = clamp_position(
            centered_x,
            static_cast<int>(work_area.left) + 20,
            static_cast<int>(work_area.right) - width - 20);
        const int y = clamp_position(
            centered_y,
            static_cast<int>(work_area.top) + 20,
            static_cast<int>(work_area.bottom) - height - 20);
        HWND designer = CreateWindowExW(
            WS_EX_DLGMODALFRAME,
            kReportTemplateDesignerClassName,
            L"Report Template Designer",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            x,
            y,
            width,
            height,
            hwnd_,
            nullptr,
            GetModuleHandleW(nullptr),
            state);
        if (!designer) {
            delete state;
            SetStatus(L"Failed to open report template designer.");
            return;
        }

        report_template_designer_ = designer;
        SetStatus(L"Report template designer opened.");
    }

    void SaveProject()
    {
        std::wstring file_name;
        if (!FileDialog::SaveProject(hwnd_, file_name)) {
            SetStatus(L"Project save canceled.");
            return;
        }

        StoreActiveObjectiveCalibration();
        const ProjectActionResult result = ProjectActions::SaveProject(
            std::filesystem::path(file_name),
            calibration_,
            measurements_,
            dye_library_,
            fluorescence_channels_,
            edf_options_,
            stitch_search_percent_,
            objective_labels_,
            objective_calibrations_,
            selected_objective_index_,
            stitch_use_orb_registration_);
        SetStatus(result.message);
    }

    void OpenProject()
    {
        std::wstring file_name;
        if (!FileDialog::OpenProject(hwnd_, file_name)) {
            SetStatus(GetLocStr(LocId::STATUS_PROJECT_OPEN_CANCELED, current_language_));
            return;
        }

        ProjectActionResult load_result = ProjectActions::LoadProject(std::filesystem::path(file_name));
        if (!load_result.succeeded) {
            SetStatus(load_result.message);
            return;
        }

        ProjectSessionRestoreResult restore = ProjectSessionRestorer::Restore(
            ProjectRuntimeState{
                calibration_,
                measurements_,
                dye_library_,
                fluorescence_channels_,
                stitch_tiles_,
                edf_stack_,
                edf_options_,
                stitch_search_percent_,
                stitch_use_orb_registration_,
                show_fusion_preview_,
                processing_retry_,
                processing_frames_,
                objective_labels_,
                objective_calibrations_,
                selected_objective_index_
            },
            std::move(load_result.session_state));
        if (HWND fusion_checkbox = GetDlgItem(hwnd_, kIdFusionPreview)) {
            SendMessageW(fusion_checkbox, BM_SETCHECK, BST_UNCHECKED, 0);
        }
        measurement_interaction_.Clear();
        edit_session_.Clear();
        if (GetCapture() == hwnd_) {
            ReleaseCapture();
        }
        InitializeDyeCombo(GetDlgItem(hwnd_, kIdDyeCombo));
        SyncSelectedDyeControls(
            GetDlgItem(hwnd_, kIdDyeCombo),
            GetDlgItem(hwnd_, kIdDyeNameEdit),
            GetDlgItem(hwnd_, kIdDyeExcitationEdit),
            GetDlgItem(hwnd_, kIdDyeEmissionEdit),
            GetDlgItem(hwnd_, kIdDyeRedEdit),
            GetDlgItem(hwnd_, kIdDyeGreenEdit),
            GetDlgItem(hwnd_, kIdDyeBlueEdit));
        if (HWND edf_radius_edit = GetDlgItem(hwnd_, kIdEdfRadiusEdit)) {
            const std::wstring radius_text = std::to_wstring(edf_options_.focus_radius);
            SetWindowTextW(edf_radius_edit, radius_text.c_str());
        }
        stitch_overlap_percent_ = ProcessingParameterRules::OverlapPercentFromSearch(stitch_search_percent_);
        stitch_options_.overlap_percent = stitch_overlap_percent_;
        stitch_options_.registration_method = stitch_use_orb_registration_
            ? StitchRegistrationMethod::Micro
            : StitchRegistrationMethod::Phase;
        SyncStitchSettingsControls();
        RefreshMeasurementList(GetDlgItem(hwnd_, kIdResultsList));
        RefreshObjectiveCombo(GetDlgItem(hwnd_, kIdObjectiveCombo));
        SyncObjectiveNameEdit(GetDlgItem(hwnd_, kIdObjectiveNameEdit));
        SyncCalibrationStatus();
        RefreshChannelList(GetDlgItem(hwnd_, kIdChannelList));
        RefreshStitchTileList(GetDlgItem(hwnd_, kIdStitchTileList));
        SyncSelectedChannelControls(
            GetDlgItem(hwnd_, kIdChannelList),
            GetDlgItem(hwnd_, kIdChannelVisible),
            GetDlgItem(hwnd_, kIdChannelBlackEdit),
            GetDlgItem(hwnd_, kIdChannelWhiteEdit));
        InvalidatePreviewFrameCache();
        InvalidatePreview(hwnd_);
        SetStatus(restore.status);
    }

    bool HandleLeftClick(POINT point)
    {
        if (measurement_interaction_.IsIdle()) {
            return false;
        }

        const RECT preview = GetPreviewRect(hwnd_);
        const bool point_in_preview = PtInRect(&preview, point) != FALSE;

        const ImageFrame& frame = CurrentPreviewFrame();
        const std::optional<ImagePoint> image_point = frame.IsValid()
            ? image_viewport_.ScreenToImage(preview, frame, point)
            : std::nullopt;

        return ApplyMeasurementInteractionActionResult(
            MeasurementInteractionActions::AddPoint(
                measurement_interaction_,
                measurements_,
                calibration_,
                point_in_preview,
                frame.IsValid(),
                image_point,
                pending_calibration_length_,
                pending_calibration_unit_,
                DisplayUnit()));
    }

private:
    MeasurementToolStartResult CurrentMeasurementToolAvailability(MeasurementToolStartKind kind) const
    {
        return MeasurementToolAvailability::ForFrame(CurrentPreviewFrame(), kind);
    }

    bool ApplyMeasurementInteractionActionResult(const MeasurementInteractionActionResult& result)
    {
        if (!result.handled) {
            return false;
        }
        if (result.measurement_list_changed) {
            RefreshMeasurementList(GetDlgItem(hwnd_, kIdResultsList));
        }
        if (result.calibration_changed) {
            StoreActiveObjectiveCalibration();
            SyncCalibrationStatus();
        }
        if (!result.message.empty()) {
            SetStatus(result.message);
        }
        if (result.preview_changed) {
            InvalidatePreview(hwnd_);
        }
        return true;
    }

    void PublishProcessingResult(ProcessingJobResult result)
    {
        processing_state_.Publish(std::move(result));
        PostMessageW(hwnd_, kMsgProcessingFinished, 0, 0);
    }

    void WaitForProcessingWorker()
    {
        if (processing_worker_.joinable()) {
            processing_worker_.join();
        }
        processing_state_.MarkIdle();
    }

    void RequestProcessingCancel()
    {
        processing_state_.RequestCancel();
    }

    bool StartStitchProcessing(
        std::vector<StitchTile> tiles,
        StitchProcessingOptions options,
        bool remember_snapshot)
    {
        const ProcessingStartActionResult start = ProcessingStartActions::StartStitch(
            processing_state_,
            processing_retry_,
            tiles,
            options,
            remember_snapshot,
            [this]() { WaitForProcessingWorker(); });
        if (!start.can_start) {
            SetStatus(start.message);
            return false;
        }

        processing_worker_ = ProcessingWorkerActions::StartStitch(
            start.launch,
            std::move(tiles),
            options,
            [this](const std::wstring& message) { SetStatus(message); },
            [this](ProcessingJobResult result) { PublishProcessingResult(std::move(result)); });
        SetStatus(start.message);
        return true;
    }
    bool StartEdfProcessing(std::vector<ImageFrame> stack, EdfOptions options, bool remember_snapshot)
    {
        const ProcessingStartActionResult start = ProcessingStartActions::StartEdf(
            processing_state_,
            processing_retry_,
            stack,
            options,
            remember_snapshot,
            [this]() { WaitForProcessingWorker(); });
        if (!start.can_start) {
            SetStatus(start.message);
            return false;
        }

        processing_worker_ = ProcessingWorkerActions::StartEdf(
            start.launch,
            std::move(stack),
            options,
            [this](const std::wstring& message) { SetStatus(message); },
            [this](ProcessingJobResult result) { PublishProcessingResult(std::move(result)); });
        SetStatus(start.message);
        return true;
    }

public:
    void InvalidatePreviewFrameCache()
    {
        preview_frame_cache_valid_ = false;
        preview_frame_cache_kind_ = PreviewFrameCacheKind::None;
        preview_frame_cache_source_data_ = nullptr;
    }

    void HandleFrameReady()
    {
        OnFrameReady();
    }

    void ApplyProcessingResult()
    {
        if (!processing_state_.IsRunning()) {
            WaitForProcessingWorker();
        }

        const ProcessingResultActionResult result =
            ProcessingResultActions::ApplyPending(processing_state_, processing_frames_);
        if (!result.handled) {
            return;
        }
        std::wstring status_message = result.message;
        if (result.changed) {
            image_viewport_.Reset();
            InvalidatePreviewFrameCache();
            InvalidatePreview(hwnd_);
            const ProcessingResultDisplaySource display_source = processing_frames_.DisplaySource();
            if (display_source == ProcessingResultDisplaySource::Stitch) {
                const std::wstring save_message =
                    SaveVisibleProcessingResult(L"stitch", L"stitch", L"Stitch result");
                if (!save_message.empty()) {
                    status_message += L" " + save_message;
                }
            } else if (display_source == ProcessingResultDisplaySource::EdfComposite) {
                const std::wstring save_message =
                    SaveVisibleProcessingResult(L"edf", L"edf", L"EDF image");
                if (!save_message.empty()) {
                    status_message += L" " + save_message;
                }
            }
        }
        SetStatus(status_message);
    }

private:
    static std::filesystem::path ApplicationDirectory()
    {
        wchar_t module_path[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameW(nullptr, module_path, MAX_PATH);
        if (length > 0 && length < MAX_PATH) {
            return std::filesystem::path(module_path).parent_path();
        }

        std::error_code error;
        std::filesystem::path base = std::filesystem::current_path(error);
        if (error) {
            base = L".";
        }
        return base;
    }

    static std::filesystem::path ProcessingOutputDirectory(const wchar_t* subdirectory)
    {
        return ApplicationDirectory() / L"exports" / subdirectory;
    }

    static std::filesystem::path NextProcessingOutputPath(
        const wchar_t* subdirectory,
        const wchar_t* file_prefix)
    {
        SYSTEMTIME now = {};
        GetLocalTime(&now);
        wchar_t file_name[96] = {};
        swprintf_s(
            file_name,
            L"%s_%04u%02u%02u_%02u%02u%02u.bmp",
            file_prefix,
            static_cast<unsigned int>(now.wYear),
            static_cast<unsigned int>(now.wMonth),
            static_cast<unsigned int>(now.wDay),
            static_cast<unsigned int>(now.wHour),
            static_cast<unsigned int>(now.wMinute),
            static_cast<unsigned int>(now.wSecond));

        const std::filesystem::path directory = ProcessingOutputDirectory(subdirectory);
        std::filesystem::path path = directory / file_name;
        if (!std::filesystem::exists(path)) {
            return path;
        }

        for (int index = 1; index < 1000; ++index) {
            wchar_t indexed_name[112] = {};
            swprintf_s(
                indexed_name,
                L"%s_%04u%02u%02u_%02u%02u%02u_%03d.bmp",
                file_prefix,
                static_cast<unsigned int>(now.wYear),
                static_cast<unsigned int>(now.wMonth),
                static_cast<unsigned int>(now.wDay),
                static_cast<unsigned int>(now.wHour),
                static_cast<unsigned int>(now.wMinute),
                static_cast<unsigned int>(now.wSecond),
                index);
            path = directory / indexed_name;
            if (!std::filesystem::exists(path)) {
                return path;
            }
        }

        return directory / (std::wstring(file_prefix) + L"_latest.bmp");
    }

    static std::filesystem::path AbsolutePathOrSelf(const std::filesystem::path& path)
    {
        std::error_code absolute_error;
        std::filesystem::path absolute_path = std::filesystem::absolute(path, absolute_error);
        return absolute_error ? path : absolute_path;
    }

    std::wstring SaveVisibleProcessingResult(
        const wchar_t* subdirectory,
        const wchar_t* file_prefix,
        const std::wstring& display_mode) const
    {
        const ImageFrame& frame = processing_frames_.ProcessingResult();
        if (!frame.IsValid()) {
            return {};
        }

        const std::filesystem::path path = NextProcessingOutputPath(subdirectory, file_prefix);
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            return L"Auto-save failed: could not create " + path.parent_path().wstring() + L".";
        }

        const ExportActionResult save_result =
            ExportActions::SaveImage(path, frame, MeasurementCollection(), display_mode, &calibration_);
        if (!save_result.saved) {
            return L"Auto-save failed: " + save_result.message;
        }

        const std::filesystem::path absolute_path = AbsolutePathOrSelf(path);
        if (processing_frames_.DisplaySource() == ProcessingResultDisplaySource::Stitch) {
            const StitchResultMetadata& metadata = processing_frames_.StitchMetadata();
            if (metadata.available) {
                std::filesystem::path metadata_path = path;
                metadata_path.replace_extension(L".txt");
                const ExportActionResult metadata_result =
                    ExportActions::SaveStitchMetadata(metadata_path, metadata);
                if (!metadata_result.saved) {
                    return L"Saved to: " + absolute_path.wstring() +
                        L". Metadata auto-save failed: " + metadata_result.message;
                }
                return L"Saved to: " + absolute_path.wstring() +
                    L". Metadata: " + AbsolutePathOrSelf(metadata_path).wstring();
            }
        }
        return L"Saved to: " + absolute_path.wstring();
    }

    static std::wstring ReadEditText(HWND edit, int max_chars)
    {
        std::wstring text(static_cast<std::size_t>(std::max(1, max_chars)), L'\0');
        if (!edit) {
            return {};
        }
        const int copied = GetWindowTextW(edit, text.data(), max_chars);
        text.resize(static_cast<std::size_t>(std::max(0, copied)));
        return text;
    }

    static std::wstring ReadWindowText(HWND control)
    {
        if (!control) {
            return {};
        }

        const int length = GetWindowTextLengthW(control);
        if (length <= 0) {
            return {};
        }

        std::wstring text(static_cast<std::size_t>(length) + 1U, L'\0');
        const int copied = GetWindowTextW(control, text.data(), length + 1);
        text.resize(static_cast<std::size_t>(std::max(0, copied)));
        return text;
    }

    static std::filesystem::path EnsureFileExtension(
        const std::filesystem::path& path,
        const std::wstring& extension)
    {
        if (!path.extension().empty()) {
            return path;
        }
        std::filesystem::path result = path;
        result += extension;
        return result;
    }

    static std::filesystem::path ReportImagePathFor(const std::filesystem::path& report_path)
    {
        std::wstring stem = report_path.stem().wstring();
        if (stem.empty()) {
            stem = L"CameraViewReport";
        }
        return report_path.parent_path() / (stem + L"_image.png");
    }

    static std::wstring AbsolutePathText(const std::filesystem::path& path)
    {
        std::error_code error;
        const std::filesystem::path absolute = std::filesystem::absolute(path, error);
        return error ? path.wstring() : absolute.wstring();
    }

    static void SetDesignerStatus(HWND status, const std::wstring& text)
    {
        if (status) {
            SetWindowTextW(status, text.c_str());
        }
    }

    static void SetCheckbox(HWND checkbox, bool checked)
    {
        if (checkbox) {
            SendMessageW(checkbox, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
        }
    }

    static bool CheckboxChecked(HWND checkbox)
    {
        return checkbox && SendMessageW(checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
    }

    static void SyncTemplateDesignerControlAvailability(ReportTemplateDesignerState* state)
    {
        if (!state) {
            return;
        }

        const BOOL image_enabled = CheckboxChecked(state->image_checkbox) ? TRUE : FALSE;
        EnableWindow(state->image_size_label, image_enabled);
        EnableWindow(state->image_size_original, image_enabled);
        EnableWindow(state->image_size_fit, image_enabled);
        EnableWindow(state->image_size_compact, image_enabled);
        EnableWindow(state->image_caption_label, image_enabled);
        EnableWindow(state->image_caption_edit, image_enabled);

        const BOOL info_enabled = CheckboxChecked(state->info_checkbox) ? TRUE : FALSE;
        EnableWindow(state->info_label, info_enabled);
        EnableWindow(state->info_edit, info_enabled);

        const BOOL notes_enabled = CheckboxChecked(state->notes_checkbox) ? TRUE : FALSE;
        EnableWindow(state->notes_label, notes_enabled);
        EnableWindow(state->notes_edit, notes_enabled);

        const BOOL table_enabled = CheckboxChecked(state->table_checkbox) ? TRUE : FALSE;
        EnableWindow(state->raw_values_checkbox, table_enabled);
        EnableWindow(state->group_measurements_checkbox, table_enabled);
        EnableWindow(state->measurement_precision_label, table_enabled);
        EnableWindow(state->measurement_precision_auto, table_enabled);
        EnableWindow(state->measurement_precision_two, table_enabled);
        EnableWindow(state->measurement_precision_three, table_enabled);

        const BOOL footer_enabled = CheckboxChecked(state->footer_checkbox) ? TRUE : FALSE;
        EnableWindow(state->footer_text_label, footer_enabled);
        EnableWindow(state->footer_text_edit, footer_enabled);
    }

    static ImageReportTemplateAccent ReadVisualTemplateAccent(const ReportTemplateDesignerState* state)
    {
        if (!state) {
            return ImageReportTemplateAccent::Blue;
        }
        if (CheckboxChecked(state->accent_green)) {
            return ImageReportTemplateAccent::Green;
        }
        if (CheckboxChecked(state->accent_gold)) {
            return ImageReportTemplateAccent::Gold;
        }
        if (CheckboxChecked(state->accent_magenta)) {
            return ImageReportTemplateAccent::Magenta;
        }
        return ImageReportTemplateAccent::Blue;
    }

    static void SetVisualTemplateAccent(
        ReportTemplateDesignerState* state,
        ImageReportTemplateAccent accent)
    {
        if (!state) {
            return;
        }

        SetCheckbox(state->accent_blue, accent == ImageReportTemplateAccent::Blue);
        SetCheckbox(state->accent_green, accent == ImageReportTemplateAccent::Green);
        SetCheckbox(state->accent_gold, accent == ImageReportTemplateAccent::Gold);
        SetCheckbox(state->accent_magenta, accent == ImageReportTemplateAccent::Magenta);
    }

    static ImageReportTemplateImageSize ReadVisualTemplateImageSize(
        const ReportTemplateDesignerState* state)
    {
        if (!state) {
            return ImageReportTemplateImageSize::Original;
        }
        if (CheckboxChecked(state->image_size_fit)) {
            return ImageReportTemplateImageSize::FitPage;
        }
        if (CheckboxChecked(state->image_size_compact)) {
            return ImageReportTemplateImageSize::Compact;
        }
        return ImageReportTemplateImageSize::Original;
    }

    static void SetVisualTemplateImageSize(
        ReportTemplateDesignerState* state,
        ImageReportTemplateImageSize image_size)
    {
        if (!state) {
            return;
        }

        SetCheckbox(state->image_size_original, image_size == ImageReportTemplateImageSize::Original);
        SetCheckbox(state->image_size_fit, image_size == ImageReportTemplateImageSize::FitPage);
        SetCheckbox(state->image_size_compact, image_size == ImageReportTemplateImageSize::Compact);
    }

    static ImageReportTemplatePageLayout ReadVisualTemplatePageLayout(
        const ReportTemplateDesignerState* state)
    {
        if (!state) {
            return ImageReportTemplatePageLayout::Standard;
        }
        if (CheckboxChecked(state->page_layout_wide)) {
            return ImageReportTemplatePageLayout::Wide;
        }
        if (CheckboxChecked(state->page_layout_compact)) {
            return ImageReportTemplatePageLayout::Compact;
        }
        return ImageReportTemplatePageLayout::Standard;
    }

    static void SetVisualTemplatePageLayout(
        ReportTemplateDesignerState* state,
        ImageReportTemplatePageLayout page_layout)
    {
        if (!state) {
            return;
        }

        SetCheckbox(
            state->page_layout_standard,
            page_layout == ImageReportTemplatePageLayout::Standard);
        SetCheckbox(state->page_layout_wide, page_layout == ImageReportTemplatePageLayout::Wide);
        SetCheckbox(state->page_layout_compact, page_layout == ImageReportTemplatePageLayout::Compact);
    }

    static ImageReportTemplatePrintOrientation ReadVisualTemplatePrintOrientation(
        const ReportTemplateDesignerState* state)
    {
        if (!state) {
            return ImageReportTemplatePrintOrientation::Portrait;
        }
        if (CheckboxChecked(state->print_orientation_landscape)) {
            return ImageReportTemplatePrintOrientation::Landscape;
        }
        return ImageReportTemplatePrintOrientation::Portrait;
    }

    static void SetVisualTemplatePrintOrientation(
        ReportTemplateDesignerState* state,
        ImageReportTemplatePrintOrientation orientation)
    {
        if (!state) {
            return;
        }

        SetCheckbox(
            state->print_orientation_portrait,
            orientation == ImageReportTemplatePrintOrientation::Portrait);
        SetCheckbox(
            state->print_orientation_landscape,
            orientation == ImageReportTemplatePrintOrientation::Landscape);
    }

    static ImageReportTemplateMeasurementPrecision ReadVisualTemplateMeasurementPrecision(
        const ReportTemplateDesignerState* state)
    {
        if (!state) {
            return ImageReportTemplateMeasurementPrecision::Automatic;
        }
        if (CheckboxChecked(state->measurement_precision_two)) {
            return ImageReportTemplateMeasurementPrecision::TwoDecimals;
        }
        if (CheckboxChecked(state->measurement_precision_three)) {
            return ImageReportTemplateMeasurementPrecision::ThreeDecimals;
        }
        return ImageReportTemplateMeasurementPrecision::Automatic;
    }

    static void SetVisualTemplateMeasurementPrecision(
        ReportTemplateDesignerState* state,
        ImageReportTemplateMeasurementPrecision precision)
    {
        if (!state) {
            return;
        }

        SetCheckbox(
            state->measurement_precision_auto,
            precision == ImageReportTemplateMeasurementPrecision::Automatic);
        SetCheckbox(
            state->measurement_precision_two,
            precision == ImageReportTemplateMeasurementPrecision::TwoDecimals);
        SetCheckbox(
            state->measurement_precision_three,
            precision == ImageReportTemplateMeasurementPrecision::ThreeDecimals);
    }

    static std::vector<ImageReportTemplateSection> DefaultVisualTemplateSectionOrder()
    {
        return {
            ImageReportTemplateSection::CurrentImage,
            ImageReportTemplateSection::ReportInformation,
            ImageReportTemplateSection::ReportNotes,
            ImageReportTemplateSection::MeasurementSummary,
            ImageReportTemplateSection::MeasurementTable,
            ImageReportTemplateSection::ImageDetails};
    }

    static const wchar_t* TemplateSectionLabel(ImageReportTemplateSection section)
    {
        switch (section) {
        case ImageReportTemplateSection::CurrentImage:
            return L"Current image";
        case ImageReportTemplateSection::ReportInformation:
            return L"Report information";
        case ImageReportTemplateSection::ReportNotes:
            return L"Notes";
        case ImageReportTemplateSection::MeasurementSummary:
            return L"Measurement summary";
        case ImageReportTemplateSection::MeasurementTable:
            return L"Measurement data table";
        case ImageReportTemplateSection::ImageDetails:
            return L"Image and calibration fields";
        default:
            return L"Report section";
        }
    }

    static const wchar_t* DefaultTemplateSectionHeading(ImageReportTemplateSection section)
    {
        switch (section) {
        case ImageReportTemplateSection::CurrentImage:
            return L"Current Image";
        case ImageReportTemplateSection::ReportInformation:
            return L"Report Information";
        case ImageReportTemplateSection::ReportNotes:
            return L"Notes";
        case ImageReportTemplateSection::MeasurementSummary:
            return L"Measurement Summary";
        case ImageReportTemplateSection::MeasurementTable:
            return L"Measurement Data";
        case ImageReportTemplateSection::ImageDetails:
            return L"Image And Calibration";
        default:
            return L"Report Section";
        }
    }

    static std::wstring VisualTemplateSectionHeading(
        const ImageReportTemplateOptions& options,
        ImageReportTemplateSection section)
    {
        const std::wstring* heading = nullptr;
        switch (section) {
        case ImageReportTemplateSection::CurrentImage:
            heading = &options.current_image_heading;
            break;
        case ImageReportTemplateSection::ReportInformation:
            heading = &options.report_information_heading;
            break;
        case ImageReportTemplateSection::ReportNotes:
            heading = &options.notes_heading;
            break;
        case ImageReportTemplateSection::MeasurementSummary:
            heading = &options.measurement_summary_heading;
            break;
        case ImageReportTemplateSection::MeasurementTable:
            heading = &options.measurement_table_heading;
            break;
        case ImageReportTemplateSection::ImageDetails:
            heading = &options.image_details_heading;
            break;
        default:
            break;
        }

        if (heading && !TextInputParser::Trim(*heading).empty()) {
            return *heading;
        }
        return DefaultTemplateSectionHeading(section);
    }

    static std::wstring TemplateSectionHeadingText(
        const ReportTemplateDesignerState* state,
        ImageReportTemplateSection section)
    {
        const std::wstring* heading = nullptr;
        if (state) {
            switch (section) {
            case ImageReportTemplateSection::CurrentImage:
                heading = &state->current_image_heading;
                break;
            case ImageReportTemplateSection::ReportInformation:
                heading = &state->report_information_heading;
                break;
            case ImageReportTemplateSection::ReportNotes:
                heading = &state->notes_heading;
                break;
            case ImageReportTemplateSection::MeasurementSummary:
                heading = &state->measurement_summary_heading;
                break;
            case ImageReportTemplateSection::MeasurementTable:
                heading = &state->measurement_table_heading;
                break;
            case ImageReportTemplateSection::ImageDetails:
                heading = &state->image_details_heading;
                break;
            default:
                break;
            }
        }

        if (heading && !TextInputParser::Trim(*heading).empty()) {
            return *heading;
        }
        return DefaultTemplateSectionHeading(section);
    }

    static void SetTemplateSectionHeadingText(
        ReportTemplateDesignerState* state,
        ImageReportTemplateSection section,
        const std::wstring& heading)
    {
        if (!state) {
            return;
        }

        switch (section) {
        case ImageReportTemplateSection::CurrentImage:
            state->current_image_heading = heading;
            break;
        case ImageReportTemplateSection::ReportInformation:
            state->report_information_heading = heading;
            break;
        case ImageReportTemplateSection::ReportNotes:
            state->notes_heading = heading;
            break;
        case ImageReportTemplateSection::MeasurementSummary:
            state->measurement_summary_heading = heading;
            break;
        case ImageReportTemplateSection::MeasurementTable:
            state->measurement_table_heading = heading;
            break;
        case ImageReportTemplateSection::ImageDetails:
            state->image_details_heading = heading;
            break;
        default:
            break;
        }
    }

    static std::wstring TemplateSectionListText(
        const ReportTemplateDesignerState* state,
        ImageReportTemplateSection section)
    {
        std::wstring label = TemplateSectionLabel(section);
        const std::wstring heading = TemplateSectionHeadingText(state, section);
        if (heading != DefaultTemplateSectionHeading(section)) {
            label += L" - " + heading;
        }
        return label;
    }

    static COLORREF TemplateAccentColor(ImageReportTemplateAccent accent)
    {
        switch (accent) {
        case ImageReportTemplateAccent::Blue:
            return RGB(91, 134, 163);
        case ImageReportTemplateAccent::Green:
            return RGB(79, 138, 105);
        case ImageReportTemplateAccent::Gold:
            return RGB(163, 119, 47);
        case ImageReportTemplateAccent::Magenta:
            return RGB(164, 90, 122);
        default:
            return RGB(91, 134, 163);
        }
    }

    static COLORREF TemplateAccentSoftColor(ImageReportTemplateAccent accent)
    {
        switch (accent) {
        case ImageReportTemplateAccent::Blue:
            return RGB(237, 243, 247);
        case ImageReportTemplateAccent::Green:
            return RGB(237, 246, 241);
        case ImageReportTemplateAccent::Gold:
            return RGB(248, 242, 230);
        case ImageReportTemplateAccent::Magenta:
            return RGB(248, 237, 242);
        default:
            return RGB(237, 243, 247);
        }
    }

    static void RefreshTemplateSectionList(
        ReportTemplateDesignerState* state,
        std::size_t selected_index = 0)
    {
        if (!state || !state->section_list) {
            return;
        }

        SendMessageW(state->section_list, WM_SETREDRAW, FALSE, 0);
        SendMessageW(state->section_list, LB_RESETCONTENT, 0, 0);
        for (ImageReportTemplateSection section : state->section_order) {
            const std::wstring label = TemplateSectionListText(state, section);
            SendMessageW(state->section_list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        }
        if (!state->section_order.empty()) {
            selected_index = std::min(selected_index, state->section_order.size() - 1U);
            SendMessageW(state->section_list, LB_SETCURSEL, selected_index, 0);
        }
        SendMessageW(state->section_list, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(state->section_list, nullptr, TRUE);
    }

    static void MoveSelectedTemplateSection(ReportTemplateDesignerState* state, int delta)
    {
        if (!state || !state->section_list || state->section_order.empty()) {
            return;
        }

        const LRESULT selection = SendMessageW(state->section_list, LB_GETCURSEL, 0, 0);
        if (selection == LB_ERR) {
            return;
        }

        const int source = static_cast<int>(selection);
        const int target = source + delta;
        if (target < 0 || target >= static_cast<int>(state->section_order.size())) {
            return;
        }

        std::swap(
            state->section_order[static_cast<std::size_t>(source)],
            state->section_order[static_cast<std::size_t>(target)]);
        RefreshTemplateSectionList(state, static_cast<std::size_t>(target));
        RefreshTemplatePreview(state);
    }

    static void RefreshTemplateSectionHeadingEdit(ReportTemplateDesignerState* state)
    {
        if (!state || !state->section_heading_edit) {
            return;
        }

        const LRESULT selection = state->section_list
            ? SendMessageW(state->section_list, LB_GETCURSEL, 0, 0)
            : LB_ERR;
        const bool has_selection =
            selection != LB_ERR &&
            selection >= 0 &&
            selection < static_cast<LRESULT>(state->section_order.size());

        state->syncing_section_heading = true;
        if (has_selection) {
            const ImageReportTemplateSection section =
                state->section_order[static_cast<std::size_t>(selection)];
            const std::wstring heading = TemplateSectionHeadingText(state, section);
            SetWindowTextW(state->section_heading_edit, heading.c_str());
        } else {
            SetWindowTextW(state->section_heading_edit, L"");
        }
        state->syncing_section_heading = false;
        EnableWindow(state->section_heading_label, has_selection ? TRUE : FALSE);
        EnableWindow(state->section_heading_edit, has_selection ? TRUE : FALSE);
    }

    static void SaveSelectedTemplateSectionHeading(ReportTemplateDesignerState* state)
    {
        if (!state || !state->section_heading_edit || state->syncing_section_heading) {
            return;
        }

        const LRESULT selection = state->section_list
            ? SendMessageW(state->section_list, LB_GETCURSEL, 0, 0)
            : LB_ERR;
        if (selection == LB_ERR ||
            selection < 0 ||
            selection >= static_cast<LRESULT>(state->section_order.size())) {
            return;
        }

        const ImageReportTemplateSection section =
            state->section_order[static_cast<std::size_t>(selection)];
        SetTemplateSectionHeadingText(state, section, ReadWindowText(state->section_heading_edit));
        RefreshTemplateSectionList(state, static_cast<std::size_t>(selection));
    }

    struct TemplatePlaceholderEntry {
        const wchar_t* label;
        const wchar_t* token;
    };

    static const TemplatePlaceholderEntry* TemplatePlaceholderEntries(std::size_t& count)
    {
        static constexpr TemplatePlaceholderEntry kEntries[] = {
            {L"Generated date", L"{{Generated}}"},
            {L"Image file", L"{{ImageFile}}"},
            {L"Image size", L"{{ImageSize}}"},
            {L"Objective", L"{{Objective}}"},
            {L"Calibration state", L"{{Calibrated}}"},
            {L"Microns per pixel", L"{{MicronsPerPixel}}"},
            {L"Display unit", L"{{DisplayUnit}}"},
            {L"Total measurements", L"{{TotalMeasurements}}"},
            {L"Measurement summary", L"{{MeasurementSummary}}"},
            {L"Measurement table", L"{{MeasurementTable}}"},
            {L"Preview mode", L"{{PreviewDisplayMode}}"},
            {L"Pseudo color", L"{{PseudoColor}}"},
            {L"Fluorescence channels", L"{{FluorescenceChannels}}"},
            {L"Stitch tiles", L"{{StitchTiles}}"},
            {L"Stitch backend", L"{{StitchResultBackend}}"},
            {L"Stitch registration", L"{{StitchResultRegistration}}"},
            {L"Stitch relations", L"{{StitchResultRelations}}"},
            {L"Stitch tile positions", L"{{StitchResultTilePositions}}"},
            {L"EDF frames", L"{{EdfFrames}}"}};
        count = sizeof(kEntries) / sizeof(kEntries[0]);
        return kEntries;
    }

    static bool IsTemplateTextTarget(const ReportTemplateDesignerState* state, HWND control)
    {
        return state &&
            control &&
            (control == state->title_edit ||
             control == state->subtitle_edit ||
             control == state->image_caption_edit ||
             control == state->footer_text_edit ||
             control == state->info_edit ||
             control == state->notes_edit ||
             control == state->section_heading_edit);
    }

    static bool IsTemplateTextTargetId(int control_id)
    {
        switch (control_id) {
        case kTemplateDesignerTitleEdit:
        case kTemplateDesignerSubtitleEdit:
        case kTemplateDesignerImageCaptionEdit:
        case kTemplateDesignerFooterTextEdit:
        case kTemplateDesignerInfoEdit:
        case kTemplateDesignerNotesEdit:
        case kTemplateDesignerSectionHeadingEdit:
            return true;
        default:
            return false;
        }
    }

    static HWND ResolveTemplateTextTarget(ReportTemplateDesignerState* state)
    {
        if (!state) {
            return nullptr;
        }

        HWND focus = GetFocus();
        if (IsTemplateTextTarget(state, focus)) {
            state->last_text_target = focus;
            return focus;
        }
        if (IsTemplateTextTarget(state, state->last_text_target)) {
            return state->last_text_target;
        }
        return state->image_caption_edit ? state->image_caption_edit : state->title_edit;
    }

    static void InsertTemplatePlaceholder(
        ReportTemplateDesignerState* state,
        const std::wstring& token)
    {
        HWND target = ResolveTemplateTextTarget(state);
        if (!state || !target || token.empty()) {
            return;
        }

        state->last_text_target = target;
        SetFocus(target);
        SendMessageW(target, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(token.c_str()));
        if (target == state->section_heading_edit) {
            SaveSelectedTemplateSectionHeading(state);
        }
        RefreshTemplatePreview(state);
        SetDesignerStatus(state->status, L"Placeholder inserted.");
    }

    static void ShowTemplatePlaceholderMenu(HWND hwnd, ReportTemplateDesignerState* state)
    {
        if (!state) {
            return;
        }

        std::size_t placeholder_count = 0;
        const TemplatePlaceholderEntry* placeholders =
            TemplatePlaceholderEntries(placeholder_count);
        HMENU menu = CreatePopupMenu();
        if (!menu) {
            SetDesignerStatus(state->status, L"Could not open placeholder menu.");
            return;
        }

        for (std::size_t index = 0; index < placeholder_count; ++index) {
            AppendMenuW(
                menu,
                MF_STRING,
                kTemplatePlaceholderMenuBase + static_cast<UINT>(index),
                placeholders[index].label);
        }

        RECT button_rect = {};
        HWND button = GetDlgItem(hwnd, kTemplateDesignerInsertField);
        if (!button || !GetWindowRect(button, &button_rect)) {
            GetWindowRect(hwnd, &button_rect);
        }

        const UINT command = TrackPopupMenu(
            menu,
            TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD,
            button_rect.left,
            button_rect.bottom,
            0,
            hwnd,
            nullptr);
        DestroyMenu(menu);

        if (command < kTemplatePlaceholderMenuBase) {
            return;
        }
        const std::size_t index =
            static_cast<std::size_t>(command - kTemplatePlaceholderMenuBase);
        if (index >= placeholder_count) {
            return;
        }
        InsertTemplatePlaceholder(state, placeholders[index].token);
    }

    static ImageReportTemplateOptions ReadVisualTemplateOptions(const ReportTemplateDesignerState* state)
    {
        ImageReportTemplateOptions options;
        if (!state) {
            return options;
        }

        options.title = ReadWindowText(state->title_edit);
        options.subtitle = ReadWindowText(state->subtitle_edit);
        options.accent = ReadVisualTemplateAccent(state);
        options.image_size = ReadVisualTemplateImageSize(state);
        options.page_layout = ReadVisualTemplatePageLayout(state);
        options.print_orientation = ReadVisualTemplatePrintOrientation(state);
        options.measurement_precision = ReadVisualTemplateMeasurementPrecision(state);
        options.image_caption = ReadWindowText(state->image_caption_edit);
        options.show_image = CheckboxChecked(state->image_checkbox);
        options.show_report_information = CheckboxChecked(state->info_checkbox);
        options.show_notes = CheckboxChecked(state->notes_checkbox);
        options.show_measurement_summary = CheckboxChecked(state->summary_checkbox);
        options.show_measurement_table = CheckboxChecked(state->table_checkbox);
        options.show_measurement_raw_values = CheckboxChecked(state->raw_values_checkbox);
        options.group_measurements_by_type = CheckboxChecked(state->group_measurements_checkbox);
        options.show_calibration_details = CheckboxChecked(state->calibration_checkbox);
        options.show_processing_details = CheckboxChecked(state->processing_checkbox);
        options.show_footer = CheckboxChecked(state->footer_checkbox);
        options.footer_text = ReadWindowText(state->footer_text_edit);
        options.report_information_fields = ReadWindowText(state->info_edit);
        options.notes = ReadWindowText(state->notes_edit);
        options.current_image_heading = state->current_image_heading;
        options.report_information_heading = state->report_information_heading;
        options.notes_heading = state->notes_heading;
        options.measurement_summary_heading = state->measurement_summary_heading;
        options.measurement_table_heading = state->measurement_table_heading;
        options.image_details_heading = state->image_details_heading;
        options.section_order = state->section_order.empty()
            ? DefaultVisualTemplateSectionOrder()
            : state->section_order;
        return options;
    }

    static void ApplyVisualTemplateOptions(
        ReportTemplateDesignerState* state,
        const ImageReportTemplateOptions& options)
    {
        if (!state) {
            return;
        }

        state->initialized = false;
        if (state->title_edit) {
            SetWindowTextW(state->title_edit, options.title.c_str());
        }
        if (state->subtitle_edit) {
            SetWindowTextW(state->subtitle_edit, options.subtitle.c_str());
        }
        SetVisualTemplateAccent(state, options.accent);
        SetVisualTemplateImageSize(state, options.image_size);
        SetVisualTemplatePageLayout(state, options.page_layout);
        SetVisualTemplatePrintOrientation(state, options.print_orientation);
        SetVisualTemplateMeasurementPrecision(state, options.measurement_precision);
        if (state->image_caption_edit) {
            SetWindowTextW(state->image_caption_edit, options.image_caption.c_str());
        }
        SetCheckbox(state->image_checkbox, options.show_image);
        SetCheckbox(state->info_checkbox, options.show_report_information);
        SetCheckbox(state->notes_checkbox, options.show_notes);
        SetCheckbox(state->summary_checkbox, options.show_measurement_summary);
        SetCheckbox(state->table_checkbox, options.show_measurement_table);
        SetCheckbox(state->raw_values_checkbox, options.show_measurement_raw_values);
        SetCheckbox(state->group_measurements_checkbox, options.group_measurements_by_type);
        SetCheckbox(state->calibration_checkbox, options.show_calibration_details);
        SetCheckbox(state->processing_checkbox, options.show_processing_details);
        SetCheckbox(state->footer_checkbox, options.show_footer);
        if (state->footer_text_edit) {
            SetWindowTextW(state->footer_text_edit, options.footer_text.c_str());
        }
        if (state->info_edit) {
            SetWindowTextW(state->info_edit, options.report_information_fields.c_str());
        }
        if (state->notes_edit) {
            SetWindowTextW(state->notes_edit, options.notes.c_str());
        }
        state->current_image_heading = options.current_image_heading;
        state->report_information_heading = options.report_information_heading;
        state->notes_heading = options.notes_heading;
        state->measurement_summary_heading = options.measurement_summary_heading;
        state->measurement_table_heading = options.measurement_table_heading;
        state->image_details_heading = options.image_details_heading;
        state->section_order = options.section_order.empty()
            ? DefaultVisualTemplateSectionOrder()
            : options.section_order;
        RefreshTemplateSectionList(state);
        RefreshTemplateSectionHeadingEdit(state);
        SyncTemplateDesignerControlAvailability(state);
        state->initialized = true;
        if (state->preview) {
            InvalidateRect(state->preview, nullptr, TRUE);
        }
    }

    static void RefreshTemplatePreview(ReportTemplateDesignerState* state)
    {
        if (state && state->initialized && state->preview) {
            InvalidateRect(state->preview, nullptr, TRUE);
        }
    }

    static std::optional<std::size_t> SelectedTemplateSectionIndex(
        const ReportTemplateDesignerState* state)
    {
        if (!state || !state->section_list) {
            return std::nullopt;
        }

        const LRESULT selection = SendMessageW(state->section_list, LB_GETCURSEL, 0, 0);
        if (selection == LB_ERR ||
            selection < 0 ||
            selection >= static_cast<LRESULT>(state->section_order.size())) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(selection);
    }

    static bool SelectTemplateSectionIndex(
        ReportTemplateDesignerState* state,
        std::size_t selected_index)
    {
        if (!state || !state->section_list || selected_index >= state->section_order.size()) {
            return false;
        }

        SendMessageW(state->section_list, LB_SETCURSEL, selected_index, 0);
        RefreshTemplateSectionHeadingEdit(state);
        RefreshTemplatePreview(state);
        return true;
    }

    static bool SelectTemplateSection(
        ReportTemplateDesignerState* state,
        ImageReportTemplateSection section)
    {
        if (!state) {
            return false;
        }

        const auto section_match = std::find(
            state->section_order.begin(),
            state->section_order.end(),
            section);
        if (section_match == state->section_order.end()) {
            return false;
        }

        return SelectTemplateSectionIndex(
            state,
            static_cast<std::size_t>(section_match - state->section_order.begin()));
    }

    static std::optional<ImageReportTemplateSection> HitTestTemplatePreviewSection(
        const ReportTemplateDesignerState* state,
        POINT point)
    {
        if (!state) {
            return std::nullopt;
        }

        for (auto area = state->preview_hit_areas.rbegin();
             area != state->preview_hit_areas.rend();
             ++area) {
            if (PtInRect(&area->bounds, point)) {
                return area->section;
            }
        }
        return std::nullopt;
    }

    static std::wstring PreviewFrameSizeText(const ImageFrame& frame)
    {
        if (!frame.IsValid()) {
            return L"No image loaded";
        }
        return std::to_wstring(frame.width) + L"x" + std::to_wstring(frame.height);
    }

    static RECT FitPreviewImageRect(const RECT& bounds, const ImageFrame& frame)
    {
        if (!frame.IsValid()) {
            return bounds;
        }

        const int bounds_width = std::max(1, static_cast<int>(bounds.right - bounds.left));
        const int bounds_height = std::max(1, static_cast<int>(bounds.bottom - bounds.top));
        const double scale = std::min(
            static_cast<double>(bounds_width) / static_cast<double>(frame.width),
            static_cast<double>(bounds_height) / static_cast<double>(frame.height));
        const int draw_width = std::max(1, static_cast<int>(frame.width * scale));
        const int draw_height = std::max(1, static_cast<int>(frame.height * scale));
        const int left = bounds.left + (bounds_width - draw_width) / 2;
        const int top = bounds.top + (bounds_height - draw_height) / 2;
        return RECT{left, top, left + draw_width, top + draw_height};
    }

    static void DrawPreviewImageThumbnail(HDC hdc, const RECT& bounds, const ImageFrame& frame)
    {
        FillSolidRect(hdc, bounds, RGB(17, 24, 32));
        HPEN border_pen = CreatePen(PS_SOLID, 1, RGB(88, 99, 112));
        HGDIOBJ old_pen = border_pen ? SelectObject(hdc, border_pen) : nullptr;
        HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, bounds.left, bounds.top, bounds.right, bounds.bottom);
        if (old_brush) {
            SelectObject(hdc, old_brush);
        }
        if (old_pen) {
            SelectObject(hdc, old_pen);
        }
        if (border_pen) {
            DeleteObject(border_pen);
        }

        if (!frame.IsValid()) {
            RECT empty_text{bounds.left + 12, bounds.top + 12, bounds.right - 12, bounds.bottom - 12};
            SetTextColor(hdc, RGB(190, 200, 210));
            DrawTextW(hdc, L"No image loaded", -1, &empty_text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return;
        }

        const RECT image_rect = FitPreviewImageRect(bounds, frame);
        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = frame.width;
        info.bmiHeader.biHeight = -frame.height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 24;
        info.bmiHeader.biCompression = BI_RGB;
        SetStretchBltMode(hdc, COLORONCOLOR);
        StretchDIBits(
            hdc,
            image_rect.left,
            image_rect.top,
            image_rect.right - image_rect.left,
            image_rect.bottom - image_rect.top,
            0,
            0,
            frame.width,
            frame.height,
            frame.bgr.data(),
            &info,
            DIB_RGB_COLORS,
            SRCCOPY);
    }
    static std::wstring PreviewMeasurementSummaryText(const CameraPreviewApp* app)
    {
        if (!app) {
            return L"No application data.";
        }

        const MeasurementCollection& measurements = app->measurements_;
        std::wostringstream text;
        text << L"Objective: " << app->ActiveObjectiveLabel() << L"\n";
        text << L"Calibration: "
             << MeasurementDisplayActions::CalibrationStatusLine(app->ActiveObjectiveLabel(), app->calibration_)
             << L"\n";
        text << L"Measurements: " << measurements.Count()
             << L" total, " << measurements.LengthCount() << L" lengths, "
             << measurements.AngleCount() << L" angles, "
             << measurements.RectangleCount() << L" rectangle areas, "
             << measurements.PolygonCount() << L" polygon areas.";
        return text.str();
    }

    static std::wstring PreviewMeasurementTableText(
        const CameraPreviewApp* app,
        const ImageReportTemplateOptions& options)
    {
        const MeasurementCollection* measurements = app ? &app->measurements_ : nullptr;
        std::wstring table_detail = measurements && !measurements->Empty()
            ? L"Measurement rows from the current image."
            : L"No measurements yet. The table will fill when measurements are added.";
        table_detail += options.show_measurement_raw_values
            ? L"\nColumns: name, type, value, unit and raw pixel value."
            : L"\nColumns: name, type, value and unit.";
        if (options.measurement_precision ==
            ImageReportTemplateMeasurementPrecision::TwoDecimals) {
            table_detail += L"\nValues use 2 decimal places.";
        } else if (options.measurement_precision ==
            ImageReportTemplateMeasurementPrecision::ThreeDecimals) {
            table_detail += L"\nValues use 3 decimal places.";
        } else {
            table_detail += L"\nValues use automatic precision.";
        }
        if (options.group_measurements_by_type) {
            table_detail += L"\nGrouped by measurement type.";
        }
        return table_detail;
    }

    static std::wstring PreviewImageDetailsText(const CameraPreviewApp* app)
    {
        if (!app) {
            return L"No image details.";
        }

        const ImageFrame& frame = app->CurrentPreviewFrame();
        std::wostringstream text;
        text << L"Image: " << PreviewFrameSizeText(frame) << L"\n";
        text << L"Objective: " << app->ActiveObjectiveLabel() << L"\n";
        text << L"Pseudo color: " << PseudoColorMapper::Label(app->pseudo_color_palette_) << L"\n";
        text << L"Fluorescence channels: " << app->fluorescence_channels_.size()
             << L", stitch tiles: " << app->stitch_tiles_.size()
             << L", EDF frames: " << app->edf_stack_.size();
        return text.str();
    }

    void ApplyDesignedReportTemplate(ReportTemplateDesignerState* state)
    {
        if (!state) {
            return;
        }

        const ImageReportTemplateOptions options = ReadVisualTemplateOptions(state);
        report_template_text_ = DiagnosticReportActions::BuildImageReportTemplate(options);
        report_template_path_.clear();
        visual_report_template_options_ = options;
        SyncReportTemplateStatus();
        SetDesignerStatus(state->status, L"Visual template applied.");
        SetStatus(GetLocStr(LocId::STATUS_REPORT_TEMPLATE_APPLIED, current_language_));
    }

    void SaveDesignedReportTemplate(HWND owner, ReportTemplateDesignerState* state)
    {
        if (!state) {
            return;
        }

        const ImageReportTemplateOptions options = ReadVisualTemplateOptions(state);
        const std::wstring template_text =
            DiagnosticReportActions::BuildImageReportTemplate(options);
        std::wstring file_name;
        if (!FileDialog::SaveTemplate(owner, file_name)) {
            SetDesignerStatus(state->status, L"Template save canceled.");
            SetStatus(L"Report template save canceled.");
            return;
        }

        const std::filesystem::path path = EnsureFileExtension(std::filesystem::path(file_name), L".html");
        const ExportActionResult result = ExportActions::SaveReportTemplate(path, template_text);
        SetDesignerStatus(state->status, result.message);
        SetStatus(result.message);
        if (result.saved) {
            report_template_text_ = template_text;
            report_template_path_ = path.wstring();
            visual_report_template_options_ = options;
            SyncReportTemplateStatus();
        }
    }

    static void DrawTemplatePreview(HWND hwnd, ReportTemplateDesignerState* state)
    {
        PAINTSTRUCT paint = {};
        HDC hdc = BeginPaint(hwnd, &paint);
        RECT client = {};
        GetClientRect(hwnd, &client);
        FillSolidRect(hdc, client, RGB(228, 233, 238));

        if (state) {
            state->preview_hit_areas.clear();
        }

        const ImageReportTemplateOptions options = ReadVisualTemplateOptions(state);
        const CameraPreviewApp* app = state ? state->app : nullptr;
        int preview_side_margin = 24;
        if (options.page_layout == ImageReportTemplatePageLayout::Wide) {
            preview_side_margin = 12;
        } else if (options.page_layout == ImageReportTemplatePageLayout::Compact) {
            preview_side_margin = 54;
        }

        RECT page = client;
        page.left += preview_side_margin;
        page.top += 18;
        page.right -= preview_side_margin;
        page.bottom -= 18;
        if (page.right <= page.left || page.bottom <= page.top) {
            EndPaint(hwnd, &paint);
            return;
        }

        FillSolidRect(hdc, page, RGB(255, 255, 255));
        HPEN border_pen = CreatePen(PS_SOLID, 1, RGB(184, 193, 202));
        HGDIOBJ old_pen = border_pen ? SelectObject(hdc, border_pen) : nullptr;
        HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, page.left, page.top, page.right, page.bottom);
        if (old_brush) {
            SelectObject(hdc, old_brush);
        }
        if (old_pen) {
            SelectObject(hdc, old_pen);
        }
        if (border_pen) {
            DeleteObject(border_pen);
        }

        const std::wstring title = options.title.empty()
            ? L"CameraView Image Report"
            : options.title;
        const COLORREF accent_color = TemplateAccentColor(options.accent);
        const COLORREF accent_soft_color = TemplateAccentSoftColor(options.accent);
        const auto selected_index = SelectedTemplateSectionIndex(state);
        const ImageReportTemplateSection selected_section = selected_index.has_value()
            ? state->section_order[*selected_index]
            : ImageReportTemplateSection::CurrentImage;

        HFONT title_font = CreateFontW(
            24,
            0,
            0,
            0,
            FW_SEMIBOLD,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS,
            L"Segoe UI");
        HFONT section_font = CreateFontW(
            17,
            0,
            0,
            0,
            FW_SEMIBOLD,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS,
            L"Segoe UI");
        HGDIOBJ old_font = title_font ? SelectObject(hdc, title_font) : nullptr;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(31, 41, 51));

        int y = page.top + 24;
        RECT text_rect{page.left + 28, y, page.right - 28, y + 34};
        DrawTextW(hdc, title.c_str(), -1, &text_rect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        y += 34;
        if (old_font) {
            SelectObject(hdc, old_font);
        }

        if (!TextInputParser::Trim(options.subtitle).empty()) {
            RECT subtitle_rect{page.left + 28, y, page.right - 28, y + 42};
            SetTextColor(hdc, RGB(63, 83, 101));
            DrawTextW(
                hdc,
                options.subtitle.c_str(),
                -1,
                &subtitle_rect,
                DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);
            y += 42;
        }

        RECT meta_rect{page.left + 28, y, page.right - 28, y + 22};
        SetTextColor(hdc, RGB(96, 112, 128));
        const std::wstring orientation_text =
            options.print_orientation == ImageReportTemplatePrintOrientation::Landscape
                ? L"Preview uses current data | Print: Landscape"
                : L"Preview uses current data | Print: Portrait";
        DrawTextW(hdc, orientation_text.c_str(), -1, &meta_rect, DT_LEFT | DT_SINGLELINE);
        y += 26;
        RECT accent_line{page.left + 28, y, page.right - 28, y + 2};
        FillSolidRect(hdc, accent_line, accent_color);
        y += 18;

        auto draw_selection_outline = [&](const RECT& bounds, bool selected) {
            if (!selected) {
                return;
            }
            HPEN selected_pen = CreatePen(PS_SOLID, 2, accent_color);
            HGDIOBJ previous_pen = selected_pen ? SelectObject(hdc, selected_pen) : nullptr;
            HGDIOBJ previous_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, bounds.left, bounds.top, bounds.right, bounds.bottom);
            if (previous_brush) {
                SelectObject(hdc, previous_brush);
            }
            if (previous_pen) {
                SelectObject(hdc, previous_pen);
            }
            if (selected_pen) {
                DeleteObject(selected_pen);
            }
        };

        auto draw_image_section = [&](
            ImageReportTemplateSection section,
            const std::wstring& label,
            const std::wstring& caption,
            int height,
            const ImageFrame& frame) {
            if (y + height + 12 > page.bottom - 26) {
                return;
            }
            const int section_top = y;
            if (section_font) {
                SelectObject(hdc, section_font);
            }
            SetTextColor(hdc, RGB(31, 41, 51));
            RECT header{page.left + 28, y, page.right - 28, y + 24};
            DrawTextW(hdc, label.c_str(), -1, &header, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            y += 28;

            RECT block{page.left + 28, y, page.right - 28, y + height};
            FillSolidRect(hdc, block, RGB(248, 250, 252));
            HPEN block_pen = CreatePen(PS_SOLID, 1, RGB(202, 211, 220));
            HGDIOBJ previous_pen = block_pen ? SelectObject(hdc, block_pen) : nullptr;
            HGDIOBJ previous_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, block.left, block.top, block.right, block.bottom);
            if (previous_brush) {
                SelectObject(hdc, previous_brush);
            }
            if (previous_pen) {
                SelectObject(hdc, previous_pen);
            }
            if (block_pen) {
                DeleteObject(block_pen);
            }

            RECT image_rect{block.left + 14, block.top + 12, block.right - 14, block.bottom - 34};
            if (image_rect.bottom > image_rect.top + 24) {
                DrawPreviewImageThumbnail(hdc, image_rect, frame);
                if (frame.IsValid()) {
                    HPEN overlay_pen = CreatePen(PS_SOLID, 2, accent_color);
                    HGDIOBJ previous_overlay_pen = overlay_pen ? SelectObject(hdc, overlay_pen) : nullptr;
                    HGDIOBJ previous_overlay_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
                    MoveToEx(hdc, image_rect.left + 24, image_rect.bottom - 24, nullptr);
                    LineTo(hdc, image_rect.right - 24, image_rect.top + 24);
                    Ellipse(hdc, image_rect.left + 20, image_rect.bottom - 28, image_rect.left + 28, image_rect.bottom - 20);
                    Ellipse(hdc, image_rect.right - 28, image_rect.top + 20, image_rect.right - 20, image_rect.top + 28);
                    if (previous_overlay_brush) {
                        SelectObject(hdc, previous_overlay_brush);
                    }
                    if (previous_overlay_pen) {
                        SelectObject(hdc, previous_overlay_pen);
                    }
                    if (overlay_pen) {
                        DeleteObject(overlay_pen);
                    }
                }
            }

            if (old_font) {
                SelectObject(hdc, old_font);
            }
            SetTextColor(hdc, RGB(96, 112, 128));
            RECT caption_rect{block.left + 14, block.bottom - 25, block.right - 14, block.bottom - 8};
            DrawTextW(hdc, caption.c_str(), -1, &caption_rect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

            RECT hit_bounds{page.left + 24, section_top - 2, page.right - 24, block.bottom + 4};
            if (state) {
                state->preview_hit_areas.push_back(
                    ReportTemplateDesignerState::PreviewSectionHitArea{section, hit_bounds});
            }
            draw_selection_outline(
                hit_bounds,
                selected_index.has_value() && selected_section == section);
            y += height + 18;
        };
        auto draw_section = [&](
            ImageReportTemplateSection section,
            const std::wstring& label,
            const std::wstring& detail,
            int height) {
            if (y + height + 12 > page.bottom - 26) {
                return;
            }
            const int section_top = y;
            if (section_font) {
                SelectObject(hdc, section_font);
            }
            SetTextColor(hdc, RGB(31, 41, 51));
            RECT header{page.left + 28, y, page.right - 28, y + 24};
            DrawTextW(hdc, label.c_str(), -1, &header, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            y += 28;

            RECT block{page.left + 28, y, page.right - 28, y + height};
            FillSolidRect(hdc, block, accent_soft_color);
            HPEN block_pen = CreatePen(PS_SOLID, 1, RGB(202, 211, 220));
            HGDIOBJ previous_pen = block_pen ? SelectObject(hdc, block_pen) : nullptr;
            HGDIOBJ previous_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, block.left, block.top, block.right, block.bottom);
            if (previous_brush) {
                SelectObject(hdc, previous_brush);
            }
            if (previous_pen) {
                SelectObject(hdc, previous_pen);
            }
            if (block_pen) {
                DeleteObject(block_pen);
            }

            if (old_font) {
                SelectObject(hdc, old_font);
            }
            SetTextColor(hdc, RGB(96, 112, 128));
            RECT detail_rect{block.left + 14, block.top + 12, block.right - 14, block.bottom - 12};
            DrawTextW(
                hdc,
                detail.c_str(),
                -1,
                &detail_rect,
                DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);

            RECT hit_bounds{page.left + 24, section_top - 2, page.right - 24, block.bottom + 4};
            if (state) {
                state->preview_hit_areas.push_back(
                    ReportTemplateDesignerState::PreviewSectionHitArea{section, hit_bounds});
            }
            draw_selection_outline(
                hit_bounds,
                selected_index.has_value() && selected_section == section);
            y += height + 18;
        };

        for (ImageReportTemplateSection section : options.section_order) {
            switch (section) {
            case ImageReportTemplateSection::CurrentImage:
                if (options.show_image) {
                    int image_preview_height = 146;
                    if (options.image_size == ImageReportTemplateImageSize::FitPage) {
                        image_preview_height = 172;
                    } else if (options.image_size == ImageReportTemplateImageSize::Compact) {
                        image_preview_height = 112;
                    }
                    const ImageFrame& preview_frame = app ? app->CurrentPreviewFrame() : ImageFrame{};
                    const std::wstring image_caption =
                        TextInputParser::Trim(options.image_caption).empty()
                            ? L"Current image: " + PreviewFrameSizeText(preview_frame)
                            : options.image_caption;
                    draw_image_section(
                        section,
                        VisualTemplateSectionHeading(options, section),
                        image_caption,
                        image_preview_height,
                        preview_frame);
                }
                break;
            case ImageReportTemplateSection::ReportInformation:
                if (options.show_report_information) {
                    const std::wstring fields =
                        TextInputParser::Trim(options.report_information_fields).empty()
                            ? L"No report information."
                            : options.report_information_fields;
                    draw_section(section, VisualTemplateSectionHeading(options, section), fields, 76);
                }
                break;
            case ImageReportTemplateSection::ReportNotes:
                if (options.show_notes) {
                    const std::wstring notes = TextInputParser::Trim(options.notes).empty()
                        ? L"No notes."
                        : options.notes;
                    draw_section(section, VisualTemplateSectionHeading(options, section), notes, 76);
                }
                break;
            case ImageReportTemplateSection::MeasurementSummary:
                if (options.show_measurement_summary) {
                    draw_section(
                        section,
                        VisualTemplateSectionHeading(options, section),
                        PreviewMeasurementSummaryText(app),
                        88);
                }
                break;
            case ImageReportTemplateSection::MeasurementTable:
                if (options.show_measurement_table) {
                    std::wstring table_detail = PreviewMeasurementTableText(app, options);
                    draw_section(
                        section,
                        VisualTemplateSectionHeading(options, section),
                        table_detail,
                        92);
                }
                break;
            case ImageReportTemplateSection::ImageDetails:
                if (options.show_calibration_details || options.show_processing_details) {
                    draw_section(
                        section,
                        VisualTemplateSectionHeading(options, section),
                        PreviewImageDetailsText(app),
                        96);
                }
                break;
            default:
                break;
            }
        }
        if (options.show_footer && page.bottom - y > 26) {
            RECT footer{page.left + 28, page.bottom - 38, page.right - 28, page.bottom - 18};
            SetTextColor(hdc, RGB(96, 112, 128));
            const std::wstring footer_text = TextInputParser::Trim(options.footer_text).empty()
                ? L"Generated by CameraView."
                : options.footer_text;
            DrawTextW(hdc, footer_text.c_str(), -1, &footer, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        }

        if (old_font) {
            SelectObject(hdc, old_font);
        }
        if (title_font) {
            DeleteObject(title_font);
        }
        if (section_font) {
            DeleteObject(section_font);
        }
        EndPaint(hwnd, &paint);
    }

    static bool ScrollReportTemplateDesignerLeftTo(
        HWND hwnd,
        ReportTemplateDesignerState* state,
        int target_offset)
    {
        if (!state) {
            return false;
        }

        const int old_offset = state->left_scroll_offset;
        state->left_scroll_offset = std::clamp(target_offset, 0, state->left_scroll_max);
        if (state->left_scroll_offset == old_offset) {
            return false;
        }

        LayoutReportTemplateDesigner(hwnd, state);
        InvalidateRect(hwnd, nullptr, FALSE);
        return true;
    }

    static bool ScrollReportTemplateDesignerLeftRangeIntoView(
        HWND hwnd,
        ReportTemplateDesignerState* state,
        int content_top,
        int content_height)
    {
        if (!state) {
            return false;
        }

        RECT client = {};
        GetClientRect(hwnd, &client);
        const int margin = 12;
        const int gap = 8;
        const int button_height = 26;
        const int bottom_top = static_cast<int>(client.bottom) - margin - button_height;
        const int visible_height = std::max(1, bottom_top - margin - gap);
        const int padding = 10;
        const int current_top = state->left_scroll_offset;
        const int current_bottom = current_top + visible_height;
        const int content_bottom = content_top + std::max(1, content_height);

        int target_offset = current_top;
        if (content_top < current_top + padding) {
            target_offset = content_top - padding;
        } else if (content_bottom > current_bottom - padding) {
            target_offset = content_bottom - visible_height + padding;
        }
        return ScrollReportTemplateDesignerLeftTo(hwnd, state, target_offset);
    }

    static void ScrollReportTemplateDesignerToSectionHeading(
        HWND hwnd,
        ReportTemplateDesignerState* state)
    {
        if (!state) {
            return;
        }
        ScrollReportTemplateDesignerLeftRangeIntoView(
            hwnd,
            state,
            state->section_heading_anchor_top,
            state->section_heading_anchor_height);
    }

    static bool HandleReportTemplateDesignerLeftScroll(
        HWND hwnd,
        ReportTemplateDesignerState* state,
        int scroll_request)
    {
        if (!state) {
            return false;
        }

        RECT client = {};
        GetClientRect(hwnd, &client);
        const int margin = 12;
        const int gap = 8;
        const int button_height = 26;
        const int bottom_top = static_cast<int>(client.bottom) - margin - button_height;
        const int page_size = std::max(1, bottom_top - margin - gap);
        int target_offset = state->left_scroll_offset;
        switch (scroll_request) {
        case SB_LINEUP:
            target_offset -= 36;
            break;
        case SB_LINEDOWN:
            target_offset += 36;
            break;
        case SB_PAGEUP:
            target_offset -= page_size;
            break;
        case SB_PAGEDOWN:
            target_offset += page_size;
            break;
        case SB_THUMBPOSITION:
        case SB_THUMBTRACK: {
            SCROLLINFO info = {};
            info.cbSize = sizeof(info);
            info.fMask = SIF_TRACKPOS;
            if (state->left_scroll_bar && GetScrollInfo(state->left_scroll_bar, SB_CTL, &info)) {
                target_offset = info.nTrackPos;
            }
            break;
        }
        case SB_TOP:
            target_offset = 0;
            break;
        case SB_BOTTOM:
            target_offset = state->left_scroll_max;
            break;
        default:
            return false;
        }

        return ScrollReportTemplateDesignerLeftTo(hwnd, state, target_offset);
    }
    static void LayoutReportTemplateDesigner(HWND hwnd, ReportTemplateDesignerState* state)
    {
        if (!state) {
            return;
        }

        RECT client = {};
        GetClientRect(hwnd, &client);
        const int client_right = static_cast<int>(client.right);
        const int client_bottom = static_cast<int>(client.bottom);
        const int margin = 12;
        const int gap = 8;
        const int button_height = 26;
        const int scroll_bar_width = 16;
        const int left_panel_width = std::clamp(client_right / 3, 310, 360);
        const int left_width = left_panel_width - scroll_bar_width - 6;
        const int preview_left = margin + left_panel_width + margin;
        const int bottom_top = client_bottom - margin - button_height;
        const int left_visible_height = std::max(1, bottom_top - margin - gap);
        state->left_scroll_offset = std::clamp(
            state->left_scroll_offset,
            0,
            state->left_scroll_max);
        const int scroll_offset = state->left_scroll_offset;
        const int left_clip_top = margin;
        const int left_clip_bottom = margin + left_visible_height;
        auto move_left_control = [&](HWND control, int x, int top, int width, int height, BOOL repaint = TRUE) {
            if (control) {
                const int moved_top = top - scroll_offset;
                const bool fully_visible =
                    moved_top >= left_clip_top && moved_top + height <= left_clip_bottom;
                ShowWindow(control, fully_visible ? SW_SHOW : SW_HIDE);
                MoveWindow(control, x, moved_top, width, height, repaint);
            }
        };

        int y = margin;
        if (state->title_label) {
            move_left_control(state->title_label, margin, y + 5, left_width, 20, TRUE);
        }
        y += 24;
        if (state->title_edit) {
            move_left_control(state->title_edit, margin, y, left_width, button_height, TRUE);
        }
        y += button_height + 8;

        if (state->subtitle_label) {
            move_left_control(state->subtitle_label, margin, y + 5, left_width, 20, TRUE);
        }
        y += 24;
        if (state->subtitle_edit) {
            move_left_control(state->subtitle_edit, margin, y, left_width, button_height, TRUE);
        }
        y += button_height + 8;

        if (state->page_layout_label) {
            move_left_control(state->page_layout_label, margin, y + 4, left_width, 20, TRUE);
        }
        y += 24;
        const int page_layout_width = (left_width - gap * 2) / 3;
        auto move_page_layout = [&](HWND control, int column, int width) {
            if (control) {
                move_left_control(
                    control,
                    margin + column * (page_layout_width + gap),
                    y,
                    width,
                    24,
                    TRUE);
            }
        };
        move_page_layout(state->page_layout_standard, 0, page_layout_width);
        move_page_layout(state->page_layout_wide, 1, page_layout_width);
        move_page_layout(
            state->page_layout_compact,
            2,
            left_width - page_layout_width * 2 - gap * 2);
        y += 28;

        if (state->print_orientation_label) {
            move_left_control(state->print_orientation_label, margin, y + 4, left_width, 20, TRUE);
        }
        y += 24;
        const int orientation_width = (left_width - gap) / 2;
        if (state->print_orientation_portrait) {
            move_left_control(state->print_orientation_portrait, margin, y, orientation_width, 24, TRUE);
        }
        if (state->print_orientation_landscape) {
            move_left_control(
                state->print_orientation_landscape,
                margin + orientation_width + gap,
                y,
                left_width - orientation_width - gap,
                24,
                TRUE);
        }
        y += 30;

        auto move_checkbox = [&](HWND control) {
            if (control) {
                move_left_control(control, margin, y, left_width, 24, TRUE);
            }
            y += 26;
        };
        move_checkbox(state->image_checkbox);

        if (state->image_size_label) {
            move_left_control(state->image_size_label, margin + 18, y + 4, left_width - 18, 20, TRUE);
        }
        y += 24;
        const int image_size_width = (left_width - 18 - gap * 2) / 3;
        auto move_image_size = [&](HWND control, int column, int width) {
            if (control) {
                move_left_control(
                    control,
                    margin + 18 + column * (image_size_width + gap),
                    y,
                    width,
                    24,
                    TRUE);
            }
        };
        move_image_size(state->image_size_original, 0, image_size_width);
        move_image_size(state->image_size_fit, 1, image_size_width);
        move_image_size(
            state->image_size_compact,
            2,
            left_width - 18 - image_size_width * 2 - gap * 2);
        y += 28;

        if (state->image_caption_label) {
            move_left_control(state->image_caption_label, margin + 18, y + 4, left_width - 18, 20, TRUE);
        }
        y += 24;
        if (state->image_caption_edit) {
            move_left_control(state->image_caption_edit, margin + 18, y, left_width - 18, button_height, TRUE);
        }
        y += button_height + 8;

        move_checkbox(state->info_checkbox);
        move_checkbox(state->notes_checkbox);
        move_checkbox(state->summary_checkbox);
        move_checkbox(state->table_checkbox);
        move_checkbox(state->raw_values_checkbox);
        move_checkbox(state->group_measurements_checkbox);
        if (state->measurement_precision_label) {
            move_left_control(
                state->measurement_precision_label,
                margin + 18,
                y + 4,
                left_width - 18,
                20,
                TRUE);
        }
        y += 24;
        const int precision_width = (left_width - 18 - gap * 2) / 3;
        auto move_precision = [&](HWND control, int column, int width) {
            if (control) {
                move_left_control(
                    control,
                    margin + 18 + column * (precision_width + gap),
                    y,
                    width,
                    24,
                    TRUE);
            }
        };
        move_precision(state->measurement_precision_auto, 0, precision_width);
        move_precision(state->measurement_precision_two, 1, precision_width);
        move_precision(
            state->measurement_precision_three,
            2,
            left_width - 18 - precision_width * 2 - gap * 2);
        y += 30;
        move_checkbox(state->calibration_checkbox);
        move_checkbox(state->processing_checkbox);
        move_checkbox(state->footer_checkbox);
        if (state->footer_text_label) {
            move_left_control(state->footer_text_label, margin + 18, y + 4, left_width - 18, 20, TRUE);
        }
        y += 24;
        if (state->footer_text_edit) {
            move_left_control(state->footer_text_edit, margin + 18, y, left_width - 18, button_height, TRUE);
        }
        y += button_height + 8;

        if (state->accent_label) {
            move_left_control(state->accent_label, margin, y + 4, left_width, 20, TRUE);
        }
        y += 24;
        const int accent_half_width = (left_width - gap) / 2;
        auto move_accent = [&](HWND control, int column, int row) {
            if (control) {
                move_left_control(
                    control,
                    margin + column * (accent_half_width + gap),
                    y + row * 26,
                    column == 0 ? accent_half_width : left_width - accent_half_width - gap,
                    24,
                    TRUE);
            }
        };
        move_accent(state->accent_blue, 0, 0);
        move_accent(state->accent_green, 1, 0);
        move_accent(state->accent_gold, 0, 1);
        move_accent(state->accent_magenta, 1, 1);
        y += 54;

        const int fields_height = 62;
        const int notes_height = 74;

        if (state->info_label) {
            move_left_control(state->info_label, margin, y + 4, left_width, 20, TRUE);
        }
        y += 24;
        if (state->info_edit) {
            move_left_control(state->info_edit, margin, y, left_width, fields_height, TRUE);
        }
        y += fields_height + 8;

        if (state->notes_label) {
            move_left_control(state->notes_label, margin, y + 4, left_width, 20, TRUE);
        }
        y += 24;
        if (state->notes_edit) {
            move_left_control(state->notes_edit, margin, y, left_width, notes_height, TRUE);
        }
        y += notes_height + 8;

        if (state->section_label) {
            move_left_control(state->section_label, margin, y + 4, left_width, 20, TRUE);
        }
        y += 24;
        const int list_height = 76;
        if (state->section_list) {
            move_left_control(state->section_list, margin, y, left_width, list_height, TRUE);
        }
        y += list_height + gap;
        const int half_width = (left_width - gap) / 2;
        HWND move_up = GetDlgItem(hwnd, kTemplateDesignerMoveUp);
        HWND move_down = GetDlgItem(hwnd, kTemplateDesignerMoveDown);
        if (move_up) {
            move_left_control(move_up, margin, y, half_width, button_height, TRUE);
        }
        if (move_down) {
            move_left_control(move_down, margin + half_width + gap, y, left_width - half_width - gap, button_height, TRUE);
        }
        y += button_height + gap;

        state->section_heading_anchor_top = y;
        state->section_heading_anchor_height = 24 + button_height;
        if (state->section_heading_label) {
            move_left_control(state->section_heading_label, margin, y + 4, left_width, 20, TRUE);
        }
        y += 24;
        if (state->section_heading_edit) {
            move_left_control(state->section_heading_edit, margin, y, left_width, button_height, TRUE);
        }
        y += button_height + margin;

        const int content_height = std::max(1, y - margin);
        const int new_max_scroll = std::max(0, content_height - left_visible_height);
        if (state->left_scroll_offset > new_max_scroll) {
            state->left_scroll_offset = new_max_scroll;
            state->left_scroll_max = new_max_scroll;
            LayoutReportTemplateDesigner(hwnd, state);
            return;
        }
        state->left_scroll_max = new_max_scroll;
        if (state->left_scroll_bar) {
            MoveWindow(
                state->left_scroll_bar,
                margin + left_width + 4,
                margin,
                scroll_bar_width,
                left_visible_height,
                TRUE);
            ShowWindow(state->left_scroll_bar, new_max_scroll > 0 ? SW_SHOW : SW_HIDE);
            SCROLLINFO scroll_info = {};
            scroll_info.cbSize = sizeof(scroll_info);
            scroll_info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
            scroll_info.nMin = 0;
            scroll_info.nMax = new_max_scroll + left_visible_height - 1;
            scroll_info.nPage = static_cast<UINT>(left_visible_height);
            scroll_info.nPos = state->left_scroll_offset;
            SetScrollInfo(state->left_scroll_bar, SB_CTL, &scroll_info, TRUE);
        }

        if (state->preview) {
            MoveWindow(
                state->preview,
                preview_left,
                margin,
                std::max(0, client_right - preview_left - margin),
                std::max(0, bottom_top - margin - gap),
                TRUE);
        }

        if (state->status) {
            MoveWindow(
                state->status,
                margin,
                bottom_top + 5,
                std::max(0, client_right - margin * 2 - 530),
                22,
                TRUE);
        }

        int right = client_right - margin;
        auto move_bottom_button = [&](int control_id, int width) {
            right -= width;
            HWND control = GetDlgItem(hwnd, control_id);
            if (control) {
                MoveWindow(control, right, bottom_top, width, button_height, TRUE);
            }
            right -= gap;
        };
        move_bottom_button(kTemplateDesignerClose, 80);
        move_bottom_button(kTemplateDesignerSave, 106);
        move_bottom_button(kTemplateDesignerApply, 86);
        move_bottom_button(kTemplateDesignerDefault, 90);
        move_bottom_button(kTemplateDesignerInsertField, 96);
    }

    static void RegisterReportTemplateDesignerClass()
    {
        static bool registered = false;
        if (registered) {
            return;
        }

        WNDCLASSEXW window_class = {};
        window_class.cbSize = sizeof(window_class);
        window_class.lpfnWndProc = &CameraPreviewApp::ReportTemplateDesignerWindowProc;
        window_class.hInstance = GetModuleHandleW(nullptr);
        window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        window_class.lpszClassName = kReportTemplateDesignerClassName;
        registered =
            RegisterClassExW(&window_class) != 0 ||
            GetLastError() == ERROR_CLASS_ALREADY_EXISTS;

        WNDCLASSEXW preview_class = {};
        preview_class.cbSize = sizeof(preview_class);
        preview_class.lpfnWndProc = &CameraPreviewApp::ReportTemplatePreviewWindowProc;
        preview_class.hInstance = GetModuleHandleW(nullptr);
        preview_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        preview_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        preview_class.lpszClassName = kReportTemplatePreviewClassName;
        RegisterClassExW(&preview_class);
    }

    static LRESULT CALLBACK ReportTemplatePreviewWindowProc(
        HWND hwnd,
        UINT message,
        WPARAM wparam,
        LPARAM lparam)
    {
        switch (message) {
        case WM_CREATE: {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            SetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(create ? create->lpCreateParams : nullptr));
            return 0;
        }
        case WM_PAINT: {
            auto* state =
                reinterpret_cast<ReportTemplateDesignerState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            DrawTemplatePreview(hwnd, state);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            auto* state =
                reinterpret_cast<ReportTemplateDesignerState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            POINT point = {
                static_cast<int>(static_cast<short>(LOWORD(lparam))),
                static_cast<int>(static_cast<short>(HIWORD(lparam)))
            };
            const auto section = HitTestTemplatePreviewSection(state, point);
            if (section.has_value() && SelectTemplateSection(state, *section)) {
                HWND designer = GetParent(hwnd);
                ScrollReportTemplateDesignerToSectionHeading(designer ? designer : hwnd, state);
                const std::wstring message =
                    L"Selected preview section: " + TemplateSectionHeadingText(state, *section);
                SetDesignerStatus(state->status, message);
                SetFocus(state->section_heading_edit ? state->section_heading_edit : hwnd);
                return 0;
            }
            break;
        }
        case WM_SETCURSOR: {
            if (LOWORD(lparam) == HTCLIENT) {
                auto* state =
                    reinterpret_cast<ReportTemplateDesignerState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
                POINT point = {};
                GetCursorPos(&point);
                ScreenToClient(hwnd, &point);
                if (HitTestTemplatePreviewSection(state, point).has_value()) {
                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                    return TRUE;
                }
            }
            break;
        }
        default:
            break;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    static LRESULT CALLBACK ReportTemplateDesignerWindowProc(
        HWND hwnd,
        UINT message,
        WPARAM wparam,
        LPARAM lparam)
    {
        auto* state = reinterpret_cast<ReportTemplateDesignerState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        switch (message) {
        case WM_CREATE: {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            state = static_cast<ReportTemplateDesignerState*>(create ? create->lpCreateParams : nullptr);
            if (!state || !state->app) {
                return -1;
            }
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
            state->app->report_template_designer_ = hwnd;

            HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            const DWORD button_style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON;
            const DWORD checkbox_style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX;
            auto create_button = [&](int control_id, const wchar_t* text) {
                HWND control = CreateWindowW(
                    L"BUTTON",
                    text,
                    button_style,
                    0,
                    0,
                    0,
                    0,
                    hwnd,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(control_id)),
                    GetModuleHandleW(nullptr),
                    nullptr);
                if (control) {
                    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                }
            };
            auto create_checkbox = [&](int control_id, const wchar_t* text) -> HWND {
                HWND control = CreateWindowW(
                    L"BUTTON",
                    text,
                    checkbox_style,
                    0,
                    0,
                    0,
                    0,
                    hwnd,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(control_id)),
                    GetModuleHandleW(nullptr),
                    nullptr);
                if (control) {
                    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                }
                return control;
            };
            auto create_radio = [&](int control_id, const wchar_t* text, bool starts_group = false) -> HWND {
                HWND control = CreateWindowW(
                    L"BUTTON",
                    text,
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON |
                        (starts_group ? WS_GROUP : 0),
                    0,
                    0,
                    0,
                    0,
                    hwnd,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(control_id)),
                    GetModuleHandleW(nullptr),
                    nullptr);
                if (control) {
                    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                }
                return control;
            };

            create_button(kTemplateDesignerDefault, L"Default");
            create_button(kTemplateDesignerApply, L"Apply");
            create_button(kTemplateDesignerSave, L"Save Template");
            create_button(kTemplateDesignerClose, L"Close");
            create_button(kTemplateDesignerMoveUp, L"Move Up");
            create_button(kTemplateDesignerMoveDown, L"Move Down");
            create_button(kTemplateDesignerInsertField, L"Insert Field");

            state->title_label = CreateWindowW(
                L"STATIC",
                L"Report title",
                WS_CHILD | WS_VISIBLE,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTemplateDesignerTitleLabel)),
                GetModuleHandleW(nullptr),
                nullptr);
            if (state->title_label) {
                SendMessageW(state->title_label, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            }

            state->title_edit = CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTemplateDesignerTitleEdit)),
                GetModuleHandleW(nullptr),
                nullptr);
            if (state->title_edit) {
                SendMessageW(state->title_edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                SendMessageW(state->title_edit, EM_SETLIMITTEXT, 120, 0);
            }

            state->subtitle_label = CreateWindowW(
                L"STATIC",
                L"Subtitle",
                WS_CHILD | WS_VISIBLE,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTemplateDesignerSubtitleLabel)),
                GetModuleHandleW(nullptr),
                nullptr);
            if (state->subtitle_label) {
                SendMessageW(state->subtitle_label, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            }

            state->subtitle_edit = CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTemplateDesignerSubtitleEdit)),
                GetModuleHandleW(nullptr),
                nullptr);
            if (state->subtitle_edit) {
                SendMessageW(state->subtitle_edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                SendMessageW(state->subtitle_edit, EM_SETLIMITTEXT, 180, 0);
            }

            state->page_layout_label = CreateWindowW(
                L"STATIC",
                L"Page layout",
                WS_CHILD | WS_VISIBLE,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTemplateDesignerPageLayoutLabel)),
                GetModuleHandleW(nullptr),
                nullptr);
            if (state->page_layout_label) {
                SendMessageW(state->page_layout_label, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            }
            state->page_layout_standard =
                create_radio(kTemplateDesignerPageLayoutStandard, L"Standard", true);
            state->page_layout_wide =
                create_radio(kTemplateDesignerPageLayoutWide, L"Wide");
            state->page_layout_compact =
                create_radio(kTemplateDesignerPageLayoutCompact, L"Compact");

            state->print_orientation_label = CreateWindowW(
                L"STATIC",
                L"Print",
                WS_CHILD | WS_VISIBLE,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTemplateDesignerPrintOrientationLabel)),
                GetModuleHandleW(nullptr),
                nullptr);
            if (state->print_orientation_label) {
                SendMessageW(
                    state->print_orientation_label,
                    WM_SETFONT,
                    reinterpret_cast<WPARAM>(font),
                    TRUE);
            }
            state->print_orientation_portrait =
                create_radio(kTemplateDesignerPrintOrientationPortrait, L"Portrait", true);
            state->print_orientation_landscape =
                create_radio(kTemplateDesignerPrintOrientationLandscape, L"Landscape");

            state->image_checkbox = create_checkbox(kTemplateDesignerImage, L"Current image");
            state->image_size_label = CreateWindowW(
                L"STATIC",
                L"Image size",
                WS_CHILD | WS_VISIBLE,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTemplateDesignerImageSizeLabel)),
                GetModuleHandleW(nullptr),
                nullptr);
            if (state->image_size_label) {
                SendMessageW(state->image_size_label, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            }
            state->image_size_original =
                create_radio(kTemplateDesignerImageSizeOriginal, L"Original", true);
            state->image_size_fit =
                create_radio(kTemplateDesignerImageSizeFit, L"Fit page");
            state->image_size_compact =
                create_radio(kTemplateDesignerImageSizeCompact, L"Compact");
            state->image_caption_label = CreateWindowW(
                L"STATIC",
                L"Image caption",
                WS_CHILD | WS_VISIBLE,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTemplateDesignerImageCaptionLabel)),
                GetModuleHandleW(nullptr),
                nullptr);
            if (state->image_caption_label) {
                SendMessageW(state->image_caption_label, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            }
            state->image_caption_edit = CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTemplateDesignerImageCaptionEdit)),
                GetModuleHandleW(nullptr),
                nullptr);
            if (state->image_caption_edit) {
                SendMessageW(state->image_caption_edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                SendMessageW(state->image_caption_edit, EM_SETLIMITTEXT, 240, 0);
            }
            state->info_checkbox = create_checkbox(kTemplateDesignerInfo, L"Report information");
            state->notes_checkbox = create_checkbox(kTemplateDesignerNotes, L"Notes");
            state->summary_checkbox = create_checkbox(kTemplateDesignerSummary, L"Measurement summary");
            state->table_checkbox = create_checkbox(kTemplateDesignerTable, L"Measurement data table");
            state->raw_values_checkbox = create_checkbox(kTemplateDesignerRawValues, L"Raw pixel values");
            state->group_measurements_checkbox =
                create_checkbox(kTemplateDesignerGroupMeasurements, L"Group by type");
            state->measurement_precision_label = CreateWindowW(
                L"STATIC",
                L"Value decimals",
                WS_CHILD | WS_VISIBLE,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(kTemplateDesignerMeasurementPrecisionLabel)),
                GetModuleHandleW(nullptr),
                nullptr);
            if (state->measurement_precision_label) {
                SendMessageW(
                    state->measurement_precision_label,
                    WM_SETFONT,
                    reinterpret_cast<WPARAM>(font),
                    TRUE);
            }
            state->measurement_precision_auto =
                create_radio(kTemplateDesignerMeasurementPrecisionAuto, L"Auto", true);
            state->measurement_precision_two =
                create_radio(kTemplateDesignerMeasurementPrecisionTwo, L"2");
            state->measurement_precision_three =
                create_radio(kTemplateDesignerMeasurementPrecisionThree, L"3");
            state->calibration_checkbox = create_checkbox(kTemplateDesignerCalibration, L"Calibration and image fields");
            state->processing_checkbox = create_checkbox(kTemplateDesignerProcessing, L"Image processing fields");
            state->footer_checkbox = create_checkbox(kTemplateDesignerFooter, L"Footer");
            state->footer_text_label = CreateWindowW(
                L"STATIC",
                L"Footer text",
                WS_CHILD | WS_VISIBLE,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTemplateDesignerFooterTextLabel)),
                GetModuleHandleW(nullptr),
                nullptr);
            if (state->footer_text_label) {
                SendMessageW(state->footer_text_label, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            }
            state->footer_text_edit = CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTemplateDesignerFooterTextEdit)),
                GetModuleHandleW(nullptr),
                nullptr);
            if (state->footer_text_edit) {
                SendMessageW(state->footer_text_edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                SendMessageW(state->footer_text_edit, EM_SETLIMITTEXT, 220, 0);
            }

            state->accent_label = CreateWindowW(
                L"STATIC",
                L"Accent color",
                WS_CHILD | WS_VISIBLE,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTemplateDesignerAccentLabel)),
                GetModuleHandleW(nullptr),
                nullptr);
            if (state->accent_label) {
                SendMessageW(state->accent_label, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            }
            state->accent_blue = create_radio(kTemplateDesignerAccentBlue, L"Blue", true);
            state->accent_green = create_radio(kTemplateDesignerAccentGreen, L"Green");
            state->accent_gold = create_radio(kTemplateDesignerAccentGold, L"Gold");
            state->accent_magenta = create_radio(kTemplateDesignerAccentMagenta, L"Magenta");

            state->info_label = CreateWindowW(
                L"STATIC",
                L"Information fields",
                WS_CHILD | WS_VISIBLE,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTemplateDesignerInfoLabel)),
                GetModuleHandleW(nullptr),
                nullptr);
            if (state->info_label) {
                SendMessageW(state->info_label, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            }

            state->info_edit = CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                    ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTemplateDesignerInfoEdit)),
                GetModuleHandleW(nullptr),
                nullptr);
            if (state->info_edit) {
                SendMessageW(state->info_edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                SendMessageW(state->info_edit, EM_SETLIMITTEXT, 1200, 0);
            }

            state->notes_label = CreateWindowW(
                L"STATIC",
                L"Notes text",
                WS_CHILD | WS_VISIBLE,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTemplateDesignerNotesLabel)),
                GetModuleHandleW(nullptr),
                nullptr);
            if (state->notes_label) {
                SendMessageW(state->notes_label, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            }

            state->notes_edit = CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                    ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTemplateDesignerNotesEdit)),
                GetModuleHandleW(nullptr),
                nullptr);
            if (state->notes_edit) {
                SendMessageW(state->notes_edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                SendMessageW(state->notes_edit, EM_SETLIMITTEXT, 1000, 0);
            }

            state->section_label = CreateWindowW(
                L"STATIC",
                L"Report sections",
                WS_CHILD | WS_VISIBLE,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTemplateDesignerSectionLabel)),
                GetModuleHandleW(nullptr),
                nullptr);
            if (state->section_label) {
                SendMessageW(state->section_label, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            }

            state->section_list = CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"LISTBOX",
                nullptr,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTemplateDesignerSectionList)),
                GetModuleHandleW(nullptr),
                nullptr);
            if (state->section_list) {
                SendMessageW(state->section_list, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            }

            state->section_heading_label = CreateWindowW(
                L"STATIC",
                L"Selected heading",
                WS_CHILD | WS_VISIBLE,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTemplateDesignerSectionHeadingLabel)),
                GetModuleHandleW(nullptr),
                nullptr);
            if (state->section_heading_label) {
                SendMessageW(state->section_heading_label, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            }

            state->section_heading_edit = CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTemplateDesignerSectionHeadingEdit)),
                GetModuleHandleW(nullptr),
                nullptr);
            if (state->section_heading_edit) {
                SendMessageW(state->section_heading_edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                SendMessageW(state->section_heading_edit, EM_SETLIMITTEXT, 120, 0);
            }

            state->left_scroll_bar = CreateWindowW(
                L"SCROLLBAR",
                nullptr,
                WS_CHILD | WS_VISIBLE | SBS_VERT,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTemplateDesignerLeftScrollBar)),
                GetModuleHandleW(nullptr),
                nullptr);

            state->preview = CreateWindowExW(
                WS_EX_CLIENTEDGE,
                kReportTemplatePreviewClassName,
                nullptr,
                WS_CHILD | WS_VISIBLE,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTemplateDesignerPreview)),
                GetModuleHandleW(nullptr),
                state);

            state->status = CreateWindowW(
                L"STATIC",
                L"Visual template ready.",
                WS_CHILD | WS_VISIBLE,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTemplateDesignerStatus)),
                GetModuleHandleW(nullptr),
                nullptr);
            if (state->status) {
                SendMessageW(state->status, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            }

            ApplyVisualTemplateOptions(state, state->app->visual_report_template_options_);
            LayoutReportTemplateDesigner(hwnd, state);
            return 0;
        }
        case WM_SIZE:
            LayoutReportTemplateDesigner(hwnd, state);
            return 0;
        case WM_GETMINMAXINFO: {
            auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
            if (limits) {
                limits->ptMinTrackSize.x = kReportTemplateDesignerMinWidth;
                limits->ptMinTrackSize.y = kReportTemplateDesignerMinHeight;
            }
            return 0;
        }
        case WM_VSCROLL:
            if (state && reinterpret_cast<HWND>(lparam) == state->left_scroll_bar) {
                HandleReportTemplateDesignerLeftScroll(hwnd, state, LOWORD(wparam));
                return 0;
            }
            break;
        case WM_MOUSEWHEEL:
            if (state) {
                POINT point = {
                    static_cast<int>(static_cast<short>(LOWORD(lparam))),
                    static_cast<int>(static_cast<short>(HIWORD(lparam)))
                };
                ScreenToClient(hwnd, &point);
                RECT client = {};
                GetClientRect(hwnd, &client);
                const int left_panel_width = std::clamp(static_cast<int>(client.right) / 3, 310, 360);
                if (point.x >= 0 && point.x <= 12 + left_panel_width + 12) {
                    const int wheel_delta = static_cast<short>(HIWORD(wparam));
                    const int wheel_steps = std::max(1, std::abs(wheel_delta) / WHEEL_DELTA);
                    const int target_offset = state->left_scroll_offset -
                        (wheel_delta > 0 ? 1 : -1) * wheel_steps * 72;
                    ScrollReportTemplateDesignerLeftTo(hwnd, state, target_offset);
                    return 0;
                }
            }
            break;
        case WM_COMMAND:
            if (!state || !state->app) {
                break;
            }
            if (HIWORD(wparam) == EN_SETFOCUS && IsTemplateTextTargetId(LOWORD(wparam))) {
                state->last_text_target = reinterpret_cast<HWND>(lparam);
                return 0;
            }
            if (HIWORD(wparam) == EN_CHANGE &&
                (LOWORD(wparam) == kTemplateDesignerTitleEdit ||
                 LOWORD(wparam) == kTemplateDesignerSubtitleEdit ||
                 LOWORD(wparam) == kTemplateDesignerImageCaptionEdit ||
                 LOWORD(wparam) == kTemplateDesignerFooterTextEdit ||
                 LOWORD(wparam) == kTemplateDesignerInfoEdit ||
                 LOWORD(wparam) == kTemplateDesignerNotesEdit)) {
                RefreshTemplatePreview(state);
                return 0;
            }
            if (LOWORD(wparam) == kTemplateDesignerSectionHeadingEdit &&
                HIWORD(wparam) == EN_CHANGE) {
                SaveSelectedTemplateSectionHeading(state);
                RefreshTemplatePreview(state);
                return 0;
            }
            if (LOWORD(wparam) == kTemplateDesignerSectionList &&
                HIWORD(wparam) == LBN_SELCHANGE) {
                RefreshTemplateSectionHeadingEdit(state);
                RefreshTemplatePreview(state);
                return 0;
            }
            switch (LOWORD(wparam)) {
            case kTemplateDesignerImage:
            case kTemplateDesignerInfo:
            case kTemplateDesignerNotes:
            case kTemplateDesignerSummary:
            case kTemplateDesignerTable:
            case kTemplateDesignerRawValues:
            case kTemplateDesignerGroupMeasurements:
            case kTemplateDesignerCalibration:
            case kTemplateDesignerProcessing:
            case kTemplateDesignerFooter:
            case kTemplateDesignerAccentBlue:
            case kTemplateDesignerAccentGreen:
            case kTemplateDesignerAccentGold:
            case kTemplateDesignerAccentMagenta:
            case kTemplateDesignerPageLayoutStandard:
            case kTemplateDesignerPageLayoutWide:
            case kTemplateDesignerPageLayoutCompact:
            case kTemplateDesignerPrintOrientationPortrait:
            case kTemplateDesignerPrintOrientationLandscape:
            case kTemplateDesignerMeasurementPrecisionAuto:
            case kTemplateDesignerMeasurementPrecisionTwo:
            case kTemplateDesignerMeasurementPrecisionThree:
            case kTemplateDesignerImageSizeOriginal:
            case kTemplateDesignerImageSizeFit:
            case kTemplateDesignerImageSizeCompact:
                SyncTemplateDesignerControlAvailability(state);
                RefreshTemplatePreview(state);
                return 0;
            case kTemplateDesignerDefault:
                ApplyVisualTemplateOptions(state, ImageReportTemplateOptions{});
                SetDesignerStatus(state->status, L"Default visual layout restored.");
                return 0;
            case kTemplateDesignerMoveUp:
                MoveSelectedTemplateSection(state, -1);
                return 0;
            case kTemplateDesignerMoveDown:
                MoveSelectedTemplateSection(state, 1);
                return 0;
            case kTemplateDesignerInsertField:
                ShowTemplatePlaceholderMenu(hwnd, state);
                return 0;
            case kTemplateDesignerApply:
                state->app->ApplyDesignedReportTemplate(state);
                return 0;
            case kTemplateDesignerSave:
                state->app->SaveDesignedReportTemplate(hwnd, state);
                return 0;
            case kTemplateDesignerClose:
                DestroyWindow(hwnd);
                return 0;
            default:
                break;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (state) {
                if (state->app && state->app->report_template_designer_ == hwnd) {
                    state->app->report_template_designer_ = nullptr;
                }
                delete state;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            }
            return 0;
        default:
            break;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    static bool ReadPositiveNumber(HWND edit, double& value)
    {
        return TextInputParser::TryParsePositiveDouble(ReadEditText(edit, 64), value);
    }

    static bool ReadByteValue(HWND edit, int& value)
    {
        return TextInputParser::TryParseByte(ReadEditText(edit, 32), value);
    }

    static bool DecodeUtf8(const std::string& bytes, std::wstring& text)
    {
        if (bytes.empty()) {
            text.clear();
            return true;
        }
        const int size = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            bytes.data(),
            static_cast<int>(bytes.size()),
            nullptr,
            0);
        if (size <= 0) {
            return false;
        }
        text.assign(static_cast<std::size_t>(size), L'\0');
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            bytes.data(),
            static_cast<int>(bytes.size()),
            text.data(),
            size);
        return true;
    }

    static bool DecodeAnsi(const std::string& bytes, std::wstring& text)
    {
        if (bytes.empty()) {
            text.clear();
            return true;
        }
        const int size = MultiByteToWideChar(
            CP_ACP,
            0,
            bytes.data(),
            static_cast<int>(bytes.size()),
            nullptr,
            0);
        if (size <= 0) {
            return false;
        }
        text.assign(static_cast<std::size_t>(size), L'\0');
        MultiByteToWideChar(
            CP_ACP,
            0,
            bytes.data(),
            static_cast<int>(bytes.size()),
            text.data(),
            size);
        return true;
    }

    static bool DecodeUtf16Le(const std::string& bytes, std::wstring& text)
    {
        const std::size_t first = bytes.size() >= 2U &&
            static_cast<unsigned char>(bytes[0]) == 0xFF &&
            static_cast<unsigned char>(bytes[1]) == 0xFE
                ? 2U
                : 0U;
        if (((bytes.size() - first) % 2U) != 0U) {
            return false;
        }

        text.clear();
        text.reserve((bytes.size() - first) / 2U);
        for (std::size_t index = first; index + 1U < bytes.size(); index += 2U) {
            const wchar_t value = static_cast<wchar_t>(
                static_cast<unsigned char>(bytes[index]) |
                (static_cast<unsigned char>(bytes[index + 1U]) << 8));
            text.push_back(value);
        }
        return true;
    }

    static bool ReadTextFile(
        const std::filesystem::path& path,
        std::wstring& text,
        std::wstring& error)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            error = L"Failed to open report template.";
            return false;
        }

        std::ostringstream stream;
        stream << input.rdbuf();
        std::string bytes = stream.str();
        if (bytes.size() >= 3U &&
            static_cast<unsigned char>(bytes[0]) == 0xEF &&
            static_cast<unsigned char>(bytes[1]) == 0xBB &&
            static_cast<unsigned char>(bytes[2]) == 0xBF) {
            bytes.erase(0, 3);
        }

        if (bytes.size() >= 2U &&
            static_cast<unsigned char>(bytes[0]) == 0xFF &&
            static_cast<unsigned char>(bytes[1]) == 0xFE) {
            if (!DecodeUtf16Le(bytes, text)) {
                error = L"Failed to read report template text.";
                return false;
            }
        } else if (!DecodeUtf8(bytes, text) && !DecodeAnsi(bytes, text)) {
            error = L"Failed to read report template text.";
            return false;
        }

        error.clear();
        return true;
    }

    static std::vector<CalibrationProfile> MakeDefaultObjectiveCalibrations()
    {
        return std::vector<CalibrationProfile>(
            CalibrationProfile::ObjectiveMagnificationOptions().size(),
            CalibrationProfile::Uncalibrated());
    }

    static std::vector<std::wstring> MakeDefaultObjectiveLabels()
    {
        return CalibrationProfile::ObjectiveMagnificationOptions();
    }

    static MeasurementUnit SelectedCalibrationUnit(HWND combo)
    {
        const LRESULT selection = combo ? SendMessageW(combo, CB_GETCURSEL, 0, 0) : 0;
        return CalibrationProfile::CalibrationUnitAtIndex(static_cast<int>(selection));
    }

    int NormalizeObjectiveIndex(int index) const
    {
        if (objective_labels_.empty() || index < 0 || index >= static_cast<int>(objective_labels_.size())) {
            return 0;
        }
        return index;
    }

    void EnsureObjectiveCalibrationCount()
    {
        if (objective_labels_.empty()) {
            objective_labels_ = MakeDefaultObjectiveLabels();
        }
        objective_calibrations_.resize(objective_labels_.size(), CalibrationProfile::Uncalibrated());
        selected_objective_index_ = NormalizeObjectiveIndex(selected_objective_index_);
    }

    std::wstring ActiveObjectiveLabel() const
    {
        if (objective_labels_.empty()) {
            return L"Objective";
        }
        return objective_labels_[static_cast<std::size_t>(NormalizeObjectiveIndex(selected_objective_index_))];
    }

    void StoreActiveObjectiveCalibration()
    {
        EnsureObjectiveCalibrationCount();
        objective_calibrations_[static_cast<std::size_t>(selected_objective_index_)] = calibration_;
    }

    void SyncObjectiveComboSelection(HWND combo) const
    {
        if (combo) {
            SendMessageW(combo, CB_SETCURSEL, selected_objective_index_, 0);
        }
    }

    void RefreshObjectiveCombo(HWND combo) const
    {
        if (!combo) {
            return;
        }
        SendMessageW(combo, CB_RESETCONTENT, 0, 0);
        for (const std::wstring& objective : objective_labels_) {
            SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(objective.c_str()));
        }
        SyncObjectiveComboSelection(combo);
    }

    void SyncObjectiveNameEdit(HWND edit) const
    {
        if (edit) {
            const std::wstring objective = ActiveObjectiveLabel();
            SetWindowTextW(edit, objective.c_str());
        }
    }

    bool ObjectiveLabelExists(const std::wstring& label, int ignored_index = -1) const
    {
        for (std::size_t index = 0; index < objective_labels_.size(); ++index) {
            if (static_cast<int>(index) != ignored_index && objective_labels_[index] == label) {
                return true;
            }
        }
        return false;
    }

    void SyncCalibrationStatus() const
    {
        HWND label = GetDlgItem(hwnd_, kIdCalibrationStatusLabel);
        if (!label) {
            return;
        }
        const std::wstring status =
            MeasurementDisplayActions::CalibrationStatusLine(ActiveObjectiveLabel(), calibration_);
        SetWindowTextW(label, status.c_str());
    }

    void SyncReportTemplateStatus() const
    {
        HWND label = GetDlgItem(hwnd_, kIdReportTemplateStatus);
        if (!label) {
            return;
        }

        std::wstring text = L"Template: default";
        if (!report_template_path_.empty()) {
            text = L"Template: " + std::filesystem::path(report_template_path_).filename().wstring();
        } else if (!report_template_text_.empty()) {
            text = L"Template: custom";
        }
        SetWindowTextW(label, text.c_str());
    }

    static void ApplyCameraDeviceListPresentation(
        HWND combo,
        const CameraDeviceListPresentation& presentation)
    {
        if (!combo) {
            return;
        }

        for (const std::wstring& item : presentation.items) {
            SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
        }
        if (presentation.selected_item >= 0) {
            SendMessageW(combo, CB_SETCURSEL, presentation.selected_item, 0);
        }
    }

    MeasurementUnit DisplayUnit() const
    {
        return MeasurementDisplayActions::DisplayUnit(calibration_);
    }

    DyeProfile SelectedDye(HWND combo) const
    {
        const LRESULT selection = combo ? SendMessageW(combo, CB_GETCURSEL, 0, 0) : 0;
        return FluorescenceDisplayActions::SelectedDye(dye_library_, static_cast<int>(selection));
    }

    std::optional<std::size_t> SelectedDyeIndex(HWND combo) const
    {
        if (!combo || dye_library_.empty()) {
            return std::nullopt;
        }

        const LRESULT selection = SendMessageW(combo, CB_GETCURSEL, 0, 0);
        return FluorescenceDisplayActions::SelectedDyeIndex(dye_library_, static_cast<int>(selection));
    }

    std::optional<std::size_t> SelectedChannelIndex(HWND list) const
    {
        if (!list) {
            return std::nullopt;
        }

        const LRESULT selection = SendMessageW(list, LB_GETCURSEL, 0, 0);
        return FluorescenceDisplayActions::SelectedChannelIndex(
            fluorescence_channels_,
            static_cast<int>(selection));
    }

    bool HasVisibleFusionPreviewChannel() const
    {
        for (const FluorescenceChannel& channel : fluorescence_channels_) {
            if (channel.visible && channel.frame.IsValid()) {
                return true;
            }
        }
        return false;
    }

    bool IsSamePreviewCacheSource(const ImageFrame& source) const
    {
        return preview_frame_cache_source_width_ == source.width &&
               preview_frame_cache_source_height_ == source.height &&
               preview_frame_cache_source_stride_ == source.stride &&
               preview_frame_cache_source_sequence_ == source.sequence &&
               preview_frame_cache_source_timestamp_ == source.timestamp &&
               preview_frame_cache_source_data_ == source.bgr.data();
    }

    void RecordPreviewCacheSource(const ImageFrame& source) const
    {
        preview_frame_cache_source_width_ = source.width;
        preview_frame_cache_source_height_ = source.height;
        preview_frame_cache_source_stride_ = source.stride;
        preview_frame_cache_source_sequence_ = source.sequence;
        preview_frame_cache_source_timestamp_ = source.timestamp;
        preview_frame_cache_source_data_ = source.bgr.data();
    }

    const ImageFrame& CurrentPreviewFrame() const
    {
        preview_source_frame_ = frame_buffer_.SnapshotShared();
        const ImageFrame& source = preview_source_frame_ ? *preview_source_frame_ : empty_preview_frame_;

        if (processing_frames_.IsProcessingResultVisible()) {
            return processing_frames_.ProcessingResult();
        }

        if (show_fusion_preview_ && HasVisibleFusionPreviewChannel()) {
            if (preview_frame_cache_valid_ &&
                preview_frame_cache_kind_ == PreviewFrameCacheKind::Fusion) {
                return preview_frame_cache_;
            }
            preview_frame_cache_ = PreviewDisplayActions::BuildPreviewFrame(
                source,
                processing_frames_,
                fluorescence_channels_,
                show_fusion_preview_,
                pseudo_color_palette_);
            preview_frame_cache_kind_ = PreviewFrameCacheKind::Fusion;
            preview_frame_cache_valid_ = true;
            return preview_frame_cache_;
        }

        if (!source.IsValid()) {
            return empty_preview_frame_;
        }

        if (pseudo_color_palette_ == PseudoColorPalette::Original) {
            return source;
        }

        if (preview_frame_cache_valid_ &&
            preview_frame_cache_kind_ == PreviewFrameCacheKind::PseudoColor &&
            IsSamePreviewCacheSource(source)) {
            return preview_frame_cache_;
        }

        preview_frame_cache_ = PreviewDisplayActions::BuildPreviewFrame(
            source,
            processing_frames_,
            fluorescence_channels_,
            show_fusion_preview_,
            pseudo_color_palette_);
        preview_frame_cache_kind_ = PreviewFrameCacheKind::PseudoColor;
        preview_frame_cache_valid_ = true;
        RecordPreviewCacheSource(source);
        return preview_frame_cache_;
    }

    MeasurementOverlayModel BuildMeasurementOverlayModel() const
    {
        return MeasurementDisplayActions::BuildOverlayModel(
            measurements_,
            calibration_,
            measurement_interaction_);
    }

    std::size_t MeasurementCount() const
    {
        return MeasurementDisplayActions::MeasurementCount(measurements_);
    }

    std::optional<MeasurementReference> SelectedMeasurement(HWND list) const
    {
        if (!list) {
            return std::nullopt;
        }

        const LRESULT selection = SendMessageW(list, LB_GETCURSEL, 0, 0);
        return MeasurementDisplayActions::SelectedMeasurement(
            measurements_,
            static_cast<int>(selection));
    }

    void SelectMeasurementInList(HWND list, MeasurementReference reference) const
    {
        if (!list) {
            return;
        }

        const std::size_t flat_index = measurements_.FlatIndexOf(reference);
        if (flat_index < measurements_.Count()) {
            SendMessageW(list, LB_SETCURSEL, static_cast<WPARAM>(flat_index), 0);
        }
    }

    void RefreshMeasurementList(HWND list)
    {
        if (!list) {
            return;
        }

        SendMessageW(list, LB_RESETCONTENT, 0, 0);
        for (const std::wstring& text : MeasurementDisplayActions::ListLines(measurements_, calibration_)) {
            SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        }
    }

    void RefreshChannelList(HWND list)
    {
        if (!list) {
            return;
        }

        SendMessageW(list, LB_RESETCONTENT, 0, 0);
        for (const std::wstring& text : FluorescenceDisplayActions::ChannelLines(fluorescence_channels_)) {
            SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        }
    }

    void RefreshStitchTileList(HWND list)
    {
        if (!list) {
            return;
        }

        SendMessageW(list, WM_SETREDRAW, FALSE, 0);
        SendMessageW(list, LB_RESETCONTENT, 0, 0);
        for (const std::wstring& text : StitchTileDisplayActions::TileLines(stitch_tiles_)) {
            SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        }
        SendMessageW(list, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(list, nullptr, TRUE);
    }

    void AppendStitchTileListItem(HWND list)
    {
        if (!list || stitch_tiles_.empty()) {
            return;
        }

        const LRESULT list_count = SendMessageW(list, LB_GETCOUNT, 0, 0);
        const std::size_t expected_previous_count = stitch_tiles_.size() - 1U;
        if (list_count != static_cast<LRESULT>(expected_previous_count)) {
            RefreshStitchTileList(list);
            return;
        }

        const std::wstring text =
            StitchTileDisplayActions::TileLine(stitch_tiles_.back(), expected_previous_count);
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
    }

    void ClearLatestFrame()
    {
        frame_buffer_.Clear();
        live_preview_overlay_buffer_.Clear();
        live_preview_overlay_last_sequence_ = 0;
        live_preview_overlay_last_invalidate_tick_ = 0;
        InvalidatePreviewFrameCache();
        SetLatestFrameSource(L"");
        SetPreviewTelemetry(L"");
        image_viewport_.Reset();
        InvalidatePreview(hwnd_);
    }

    void OnFrameReady()
    {
        preview_source_frame_.reset();
        if (IsLiveCameraPreviewOverlayVisible()) {
            const std::shared_ptr<const ImageFrame> live_preview =
                live_preview_overlay_buffer_.SnapshotShared();
            const DWORD now = GetTickCount();
            if (live_preview &&
                live_preview->sequence != live_preview_overlay_last_sequence_ &&
                now - live_preview_overlay_last_invalidate_tick_ >= kLivePreviewOverlayMinIntervalMs) {
                live_preview_overlay_last_sequence_ = live_preview->sequence;
                live_preview_overlay_last_invalidate_tick_ = now;
                RECT overlay = LiveCameraPreviewOverlayRect(GetPreviewRect(hwnd_));
                if (overlay.right > overlay.left && overlay.bottom > overlay.top) {
                    overlay.right += 4;
                    overlay.bottom += 4;
                    InvalidateRect(hwnd_, &overlay, FALSE);
                }
                return;
            }
            return;
        }

        InvalidatePreviewFrameCache();
        InvalidatePreview(hwnd_);
    }

    void CaptureThread()
    {
        const int requested_camera_index = selected_camera_index_;
        if (requested_camera_index < 0) {
            SetStatus(CameraControlStatusFormatter::FormatNoCameraSelected(current_language_));
            running_ = false;
            return;
        }

        if (!camera_driver_.Load()) {
            SetPreviewTelemetry(L"SDK not loaded");
            SetStatus(camera_driver_.LastError());
            running_ = false;
            return;
        }
        SetPreviewTelemetry(BuildSdkTelemetry());

        const std::vector<CameraDevice> devices = camera_driver_.EnumerateDevices();
        if (devices.empty()) {
            SetStatus(GetLocStr(LocId::STATUS_NO_CAMERA_FOUND, current_language_));
            running_ = false;
            return;
        }

        if (requested_camera_index >= static_cast<int>(devices.size())) {
            SetStatus(GetLocStr(LocId::STATUS_DEVICE_NOT_AVAILABLE, current_language_));
            running_ = false;
            return;
        }

        float requested_exposure = 10.0f;
        {
            std::lock_guard<std::mutex> lock(settings_mutex_);
            requested_exposure = requested_exposure_ms_;
        }
        if (!camera_driver_.Open(requested_camera_index, requested_exposure)) {
            SetStatus(GetLocStr(LocId::STATUS_FAILED_TO_OPEN_CAMERA, current_language_));
            running_ = false;
            return;
        }

        const CameraOpenInfo open_info = camera_driver_.OpenInfo();
        SetLatestFrameSource(
            L"Camera device " + std::to_wstring(requested_camera_index + 1) +
            L" | type " + std::to_wstring(open_info.type));

        SetStatus(CameraTelemetryFormatter::FormatPreviewStarted(requested_camera_index, open_info));
        SetPreviewTelemetry(CameraTelemetryFormatter::FormatPendingTelemetry(requested_camera_index, open_info));

        unsigned long long sequence = 0;
        int fail_count = 0;
        DWORD last_fps_tick = GetTickCount();
        DWORD last_overlay_preview_tick = 0;
        unsigned long long last_fps_sequence = 0;
        std::wstring final_status = CameraControlStatusFormatter::FormatPreviewStopped(current_language_);

        while (running_) {
            ImageFrame frame;
            if (camera_driver_.GrabFrame(sequence + 1, frame)) {
                fail_count = 0;
                sequence = frame.sequence;
                const unsigned long timestamp = frame.timestamp;
                const DWORD now = GetTickCount();
                if (now - last_overlay_preview_tick >= kLivePreviewOverlayMinIntervalMs) {
                    ImageFrame overlay_preview = BuildLivePreviewOverlayFrame(frame);
                    if (overlay_preview.IsValid()) {
                        live_preview_overlay_buffer_.Publish(std::move(overlay_preview));
                        last_overlay_preview_tick = now;
                    }
                }
                frame_buffer_.Publish(std::move(frame));
                PostMessageW(hwnd_, kMsgFrameReady, 0, 0);

                if (now - last_fps_tick >= 1000) {
                    const double fps = (sequence - last_fps_sequence) * 1000.0 / static_cast<double>(now - last_fps_tick);
                    last_fps_tick = now;
                    last_fps_sequence = sequence;

                    SetPreviewTelemetry(CameraTelemetryFormatter::FormatFrameTelemetry(
                        requested_camera_index,
                        open_info,
                        fps,
                        timestamp));
                }
            } else {
                ++fail_count;
                if (fail_count > 5) {
                    if (!camera_driver_.IsConnected()) {
                        final_status = CameraControlStatusFormatter::FormatDisconnected(current_language_);
                        break;
                    }
                    fail_count = 0;
                }
                Sleep(30);
            }
        }

        camera_driver_.Close();
        running_ = false;
        SetPreviewTelemetry(L"");
        SetStatus(final_status);
    }

    static std::wstring FormatStitchTilePositions(const StitchResultMetadata& metadata)
    {
        if (!metadata.available || metadata.tiles.empty()) {
            return {};
        }

        std::wostringstream output;
        for (std::size_t index = 0; index < metadata.tiles.size(); ++index) {
            if (index > 0) {
                output << L"; ";
            }
            const StitchResultTileMetadata& tile = metadata.tiles[index];
            output << (index + 1U)
                   << L": "
                   << tile.width << L"x" << tile.height
                   << L" @ "
                   << tile.offset_x << L"," << tile.offset_y;
            if (tile.estimated_position) {
                output << L" estimated";
            }
        }
        return output.str();
    }

    DiagnosticReportActionInput BuildDiagnosticsInput()
    {
        SYSTEMTIME now = {};
        GetLocalTime(&now);

        std::wstring status_text;
        std::wstring telemetry_text;
        std::wstring frame_source_text;
        {
            std::lock_guard<std::mutex> lock(status_mutex_);
            status_text = status_;
            telemetry_text = preview_telemetry_;
            frame_source_text = latest_frame_source_;
        }

        ImageFrame frame = frame_buffer_.Snapshot();

        DiagnosticReportActionInput input;
        input.generated = DiagnosticReportTimestamp{
            static_cast<int>(now.wYear),
            static_cast<int>(now.wMonth),
            static_cast<int>(now.wDay),
            static_cast<int>(now.wHour),
            static_cast<int>(now.wMinute),
            static_cast<int>(now.wSecond)};
        input.status = status_text;
        input.preview_telemetry = telemetry_text;
        input.viewport_zoom = ViewportInteractionActions::FormatZoomValue(image_viewport_.Zoom());
        input.preview_running = running_.load();
        input.processing_running = processing_state_.IsRunning();
        input.sdk = camera_driver_.Diagnostics();
        input.enumerated_cameras = camera_count_.load();
        input.selected_camera_index = selected_camera_index_.load();
        input.enumerated_devices = camera_devices_;
        input.latest_frame_source = frame_source_text;
        input.latest_frame = std::move(frame);
        input.objective_label = ActiveObjectiveLabel();
        input.calibration = calibration_;
        input.display_unit = DisplayUnit();
        input.preview_display_mode = PreviewDisplayActions::PreviewModeLabel(
            input.latest_frame,
            processing_frames_,
            fluorescence_channels_,
            show_fusion_preview_,
            pseudo_color_palette_);
        input.pseudo_color = pseudo_color_palette_;
        input.dye_profiles = dye_library_.size();
        input.fluorescence_channels = fluorescence_channels_.size();
        input.stitch_tiles = stitch_tiles_.size();
        input.stitch_search_percent = stitch_search_percent_;
        input.edf_frames = edf_stack_.size();
        input.edf_focus_radius = edf_options_.focus_radius;
        input.processing_result_visible = processing_frames_.IsProcessingResultVisible();
        input.edf_composite_available = processing_frames_.EdfCompositeFrame().IsValid();
        input.edf_focus_map_available = processing_frames_.EdfFocusMap().IsValid();
        const ImageFrame& processing_result = processing_frames_.ProcessingResult();
        if (processing_result.IsValid()) {
            input.processing_result_width = processing_result.width;
            input.processing_result_height = processing_result.height;
            input.processing_result_kind = processing_frames_.DisplayKindLabel();
            input.processing_result_source = processing_frames_.DisplaySourceLabel();
        }
        const StitchResultMetadata& stitch_metadata = processing_frames_.StitchMetadata();
        if (stitch_metadata.available) {
            input.stitch_result_backend = stitch_metadata.backend;
            input.stitch_result_layout = stitch_metadata.layout_mode;
            input.stitch_result_registration = stitch_metadata.registration_method;
            input.stitch_result_transform = stitch_metadata.transform_model;
            input.stitch_result_blend = stitch_metadata.blend_mode;
            input.stitch_result_overlap_percent = stitch_metadata.overlap_percent;
            input.stitch_result_tiles = stitch_metadata.tiles.size();
            input.stitch_result_relations = stitch_metadata.relation_count;
            input.stitch_result_tile_positions = FormatStitchTilePositions(stitch_metadata);
        }

        return input;
    }

    std::wstring BuildDiagnosticsReport()
    {
        DiagnosticReportActionInput input = BuildDiagnosticsInput();
        return DiagnosticReportActions::BuildReport(std::move(input), measurements_, report_template_text_);
    }

    std::wstring BuildImageReport(
        const std::wstring& image_file_name,
        const ImageFrame& report_image)
    {
        DiagnosticReportActionInput input = BuildDiagnosticsInput();
        return DiagnosticReportActions::BuildImageReport(
            std::move(input),
            measurements_,
            image_file_name,
            report_image,
            report_template_text_);
    }

    std::wstring BuildSdkTelemetry() const
    {
        return DiagnosticReportActions::BuildSdkTelemetry(camera_driver_.Diagnostics());
    }

    void SetStatus(const std::wstring& text)
    {
        {
            std::lock_guard<std::mutex> lock(status_mutex_);
            status_ = text;
        }
        PostMessageW(hwnd_, kMsgStatusChanged, 0, 0);
    }

    void SetPreviewTelemetry(const std::wstring& text)
    {
        {
            std::lock_guard<std::mutex> lock(status_mutex_);
            preview_telemetry_ = text;
        }
        PostMessageW(hwnd_, kMsgStatusChanged, 0, 0);
    }

    void SetLatestFrameSource(const std::wstring& text)
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        latest_frame_source_ = text;
    }

    static void FillSolidRect(HDC hdc, const RECT& rect, COLORREF color)
    {
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);
    }

    bool EnsurePaintBuffer(HDC reference_dc, int width, int height)
    {
        if (!paint_buffer_dc_) {
            paint_buffer_dc_ = CreateCompatibleDC(reference_dc);
            if (!paint_buffer_dc_) {
                return false;
            }
        }

        if (paint_buffer_bitmap_ &&
            paint_buffer_width_ == width &&
            paint_buffer_height_ == height) {
            return true;
        }

        if (paint_buffer_bitmap_) {
            if (paint_buffer_old_bitmap_) {
                SelectObject(paint_buffer_dc_, paint_buffer_old_bitmap_);
                paint_buffer_old_bitmap_ = nullptr;
            }
            DeleteObject(paint_buffer_bitmap_);
            paint_buffer_bitmap_ = nullptr;
        }

        paint_buffer_bitmap_ = CreateCompatibleBitmap(reference_dc, width, height);
        if (!paint_buffer_bitmap_) {
            paint_buffer_width_ = 0;
            paint_buffer_height_ = 0;
            return false;
        }

        paint_buffer_old_bitmap_ = SelectObject(paint_buffer_dc_, paint_buffer_bitmap_);
        paint_buffer_width_ = width;
        paint_buffer_height_ = height;
        return true;
    }

    void ReleasePaintBuffer()
    {
        if (paint_buffer_dc_ && paint_buffer_bitmap_) {
            if (paint_buffer_old_bitmap_) {
                SelectObject(paint_buffer_dc_, paint_buffer_old_bitmap_);
                paint_buffer_old_bitmap_ = nullptr;
            }
            DeleteObject(paint_buffer_bitmap_);
            paint_buffer_bitmap_ = nullptr;
        }
        if (paint_buffer_dc_) {
            DeleteDC(paint_buffer_dc_);
            paint_buffer_dc_ = nullptr;
        }
        paint_buffer_width_ = 0;
        paint_buffer_height_ = 0;
    }

    HWND hwnd_ = nullptr;
    MUCamCameraDriver camera_driver_;
    std::vector<CameraDevice> camera_devices_;

    std::atomic_bool running_ = false;
    std::thread worker_;
    std::thread processing_worker_;
    std::thread live_stitch_capture_worker_;
    std::thread live_stitch_preview_worker_;

    std::mutex status_mutex_;
    std::mutex settings_mutex_;
    std::mutex live_stitch_capture_mutex_;
    std::mutex live_stitch_preview_mutex_;

    FrameBuffer frame_buffer_;
    FrameBuffer live_preview_overlay_buffer_;
    mutable std::shared_ptr<const ImageFrame> preview_source_frame_;
    mutable ImageFrame empty_preview_frame_;
    mutable ImageFrame preview_frame_cache_;
    mutable bool preview_frame_cache_valid_ = false;
    mutable PreviewFrameCacheKind preview_frame_cache_kind_ = PreviewFrameCacheKind::None;
    mutable int preview_frame_cache_source_width_ = 0;
    mutable int preview_frame_cache_source_height_ = 0;
    mutable int preview_frame_cache_source_stride_ = 0;
    mutable unsigned long long preview_frame_cache_source_sequence_ = 0;
    mutable unsigned long preview_frame_cache_source_timestamp_ = 0;
    mutable const unsigned char* preview_frame_cache_source_data_ = nullptr;
    std::wstring status_ = L"Ready.";
    std::wstring preview_telemetry_;
    std::wstring latest_frame_source_;
    std::wstring report_template_text_;
    std::wstring report_template_path_;
    ImageReportTemplateOptions visual_report_template_options_;
    HWND report_template_designer_ = nullptr;
    float requested_exposure_ms_ = 10.0f;
    std::atomic_int camera_count_ = 0;
    std::atomic_int selected_camera_index_ = -1;
    ImageViewport image_viewport_;
    OverlayRenderer overlay_renderer_;
    PseudoColorPalette pseudo_color_palette_ = PseudoColorPalette::Original;
    HistogramData histogram_data_;
    HistogramChannel histogram_channel_ = HistogramChannel::Luminance;
    HistogramRenderer histo_renderer_;
    UILanguage current_language_ = UILanguage::English;
    ImageAdjustParams image_adjust_;
    mutable ImageFrame adjusted_preview_;
    mutable unsigned long long histogram_cache_seq_ = 0;
    mutable HistogramChannel histogram_cache_ch_ = static_cast<HistogramChannel>(-1);
    mutable ImageAdjustParams histogram_cache_adjust_;
    std::vector<DyeProfile> dye_library_ = DyeLibrary::DefaultDyes();
    std::vector<FluorescenceChannel> fluorescence_channels_;
    std::vector<StitchTile> stitch_tiles_;
    int stitch_search_percent_ = ProcessingParameterRules::DefaultStitchSearchPercent();
    int stitch_overlap_percent_ = ProcessingParameterRules::DefaultStitchOverlapPercent();
    bool stitch_use_orb_registration_ = true;
    StitchProcessingOptions stitch_options_;
    std::wstring stitch_source_status_ = L"(no images selected)";
    bool live_stitch_active_ = false;
    int live_stitch_interval_ms_ = kDefaultLiveStitchIntervalMs;
    bool live_stitch_tick_in_progress_ = false;
    unsigned long long live_stitch_last_evaluated_sequence_ = 0;
    std::size_t live_stitch_capture_count_ = 0;
    bool live_stitch_out_of_range_warning_ = false;
    int live_stitch_out_of_range_candidate_count_ = 0;
    int live_stitch_missing_match_count_ = 0;
    DWORD live_stitch_last_status_tick_ = 0;
    DWORD live_stitch_preview_status_tick_ = 0;
    DWORD live_stitch_last_warning_beep_tick_ = 0;
    std::wstring live_stitch_last_status_message_;
    bool live_stitch_capture_worker_running_ = false;
    bool live_stitch_capture_request_pending_ = false;
    bool live_stitch_capture_ready_ = false;
    unsigned long long live_stitch_capture_generation_ = 0;
    unsigned long long live_stitch_capture_min_generation_ = 0;
    LiveStitchCaptureRequest live_stitch_capture_pending_request_;
    LiveStitchCaptureResult live_stitch_capture_ready_result_;
    std::vector<LiveStitchPreviewTile> live_stitch_preview_tiles_;
    std::vector<LiveStitchPreviewTile> live_stitch_reference_tiles_;
    int live_stitch_preview_tile_scale_ = 0;
    bool live_stitch_preview_worker_running_ = false;
    bool live_stitch_preview_request_pending_ = false;
    bool live_stitch_preview_ready_ = false;
    unsigned long long live_stitch_preview_generation_ = 0;
    unsigned long long live_stitch_preview_min_generation_ = 0;
    unsigned long long live_stitch_preview_pending_generation_ = 0;
    unsigned long long live_stitch_preview_ready_generation_ = 0;
    std::vector<LiveStitchPreviewTile> live_stitch_preview_pending_tiles_;
    LiveStitchPreviewOptions live_stitch_preview_pending_options_;
    ImageFrame live_stitch_preview_ready_image_;
    StitchResultMetadata live_stitch_preview_ready_metadata_;
    unsigned long long live_preview_overlay_last_sequence_ = 0;
    DWORD live_preview_overlay_last_invalidate_tick_ = 0;
    std::vector<ImageFrame> edf_stack_;
    ProcessingRetryState processing_retry_;
    ProcessingResultFrames processing_frames_;
    EdfOptions edf_options_ = ProcessingParameterRules::DefaultEdfOptions();
    ProcessingJobState processing_state_;
    CalibrationProfile calibration_ = CalibrationProfile::Uncalibrated();
    std::vector<std::wstring> objective_labels_ = MakeDefaultObjectiveLabels();
    std::vector<CalibrationProfile> objective_calibrations_ = MakeDefaultObjectiveCalibrations();
    int selected_objective_index_ = 0;
    MeasurementCollection measurements_;
    MeasurementInteractionState measurement_interaction_;
    MeasurementEditSession edit_session_;
    double pending_calibration_length_ = 100.0;
    MeasurementUnit pending_calibration_unit_ = MeasurementUnit::Micrometers;
    bool show_fusion_preview_ = false;
    bool function_panel_visible_ = true;
    bool function_panel_docked_left_ = true;
    bool function_panel_drag_active_ = false;
    bool function_panel_resize_active_ = false;
    int function_panel_width_ = 0;
    int panel_category_ = 0;
    int panel_scroll_offset_ = 0;
    ViewportPanState viewport_pan_;
    HDC paint_buffer_dc_ = nullptr;
    HBITMAP paint_buffer_bitmap_ = nullptr;
    HGDIOBJ paint_buffer_old_bitmap_ = nullptr;
    int paint_buffer_width_ = 0;
    int paint_buffer_height_ = 0;
};

CameraPreviewApp* GetApp(HWND hwnd)
{
    return reinterpret_cast<CameraPreviewApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

void LayoutControls(HWND hwnd, bool repaint_children)
{
    RECT client = {};
    GetClientRect(hwnd, &client);

    CameraPreviewApp* app = GetApp(hwnd);
    const int panel_category = app ? app->PanelCategory() : 0;
    const bool function_panel_visible =
        app ? app->FunctionPanelVisible() : IsFunctionPanelVisible(hwnd);
    const bool function_panel_docked_left =
        app ? app->FunctionPanelDockedLeft() : IsFunctionPanelDockedLeft(hwnd);
    const int function_panel_width = app ? app->FunctionPanelWidth() : FunctionPanelWidth(hwnd);
    if (app) {
        app->ClampPanelScroll();
    }
    const int panel_scroll_offset = app ? app->PanelScrollOffset() : 0;
    const std::vector<WindowControlPlacement> placements =
        WindowControlLayout::Compute(
            client,
            panel_category,
            panel_scroll_offset,
            function_panel_visible,
            function_panel_docked_left,
            function_panel_width);
    std::vector<HWND> paused_redraw_controls;
    if (!repaint_children) {
        paused_redraw_controls.reserve(placements.size());
        for (const WindowControlPlacement& placement : placements) {
            HWND control = GetDlgItem(hwnd, placement.control_id);
            if (control) {
                SendMessageW(control, WM_SETREDRAW, FALSE, 0);
                paused_redraw_controls.push_back(control);
            }
        }
    }

    HDWP deferred = BeginDeferWindowPos(static_cast<int>(placements.size()));
    for (const WindowControlPlacement& placement : placements) {
        HWND control = GetDlgItem(hwnd, placement.control_id);
        if (!control) {
            continue;
        }

        UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_DEFERERASE;
        if (placement.visible) {
            flags |= SWP_SHOWWINDOW;
            if (deferred) {
                HDWP next_deferred = DeferWindowPos(
                    deferred,
                    control,
                    nullptr,
                    placement.bounds.left,
                    placement.bounds.top,
                    placement.bounds.right - placement.bounds.left,
                    placement.bounds.bottom - placement.bounds.top,
                    flags);
                if (next_deferred) {
                    deferred = next_deferred;
                    continue;
                }
                EndDeferWindowPos(deferred);
                deferred = nullptr;
            }
            SetWindowPos(
                control,
                nullptr,
                placement.bounds.left,
                placement.bounds.top,
                placement.bounds.right - placement.bounds.left,
                placement.bounds.bottom - placement.bounds.top,
                flags);
        } else {
            flags |= SWP_HIDEWINDOW | SWP_NOMOVE | SWP_NOSIZE;
            if (deferred) {
                HDWP next_deferred = DeferWindowPos(
                    deferred,
                    control,
                    nullptr,
                    0,
                    0,
                    0,
                    0,
                    flags);
                if (next_deferred) {
                    deferred = next_deferred;
                    continue;
                }
                EndDeferWindowPos(deferred);
                deferred = nullptr;
            }
            SetWindowPos(control, nullptr, 0, 0, 0, 0, flags);
        }
    }
    if (deferred) {
        EndDeferWindowPos(deferred);
    }
    for (HWND control : paused_redraw_controls) {
        SendMessageW(control, WM_SETREDRAW, TRUE, 0);
    }
    if (app) {
        app->SyncPanelScrollBar();
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE: {
        auto* app = new CameraPreviewApp(hwnd);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        DragAcceptFiles(hwnd, TRUE);

        HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        for (const WindowControlDefinition& definition : WindowControlDefinitions::All()) {
            HWND control = CreateWindowW(
                definition.class_name,
                definition.text,
                definition.style,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(definition.control_id)),
                nullptr,
                nullptr);
            if (control) {
                SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            }
        }

        // Create tooltip for toolbar icon buttons
        HWND tooltip = CreateWindowExW(
            WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
            WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            hwnd, nullptr, nullptr, nullptr);
        if (tooltip) {
            SetWindowPos(tooltip, HWND_TOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

            auto add_tool = [&](int id, const wchar_t* tip) {
                HWND btn = GetDlgItem(hwnd, id);
                if (!btn) return;
                TOOLINFOW ti = {};
                ti.cbSize = sizeof(ti);
                ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
                ti.hwnd = hwnd;
                ti.uId = reinterpret_cast<UINT_PTR>(btn);
                ti.lpszText = const_cast<wchar_t*>(tip);
                SendMessageW(tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
                SendMessageW(tooltip, TTM_SETMAXTIPWIDTH, 0, 300);
            };

            add_tool(kIdFitView, L"Fit to View");
            add_tool(kIdToggleFunctionPanel, L"Show/Hide Panel");
            add_tool(kIdTogglePanelDock, L"Dock Left/Right");
        }

        HWND device_combo = GetDlgItem(hwnd, kIdDeviceCombo);
        HWND objective_combo = GetDlgItem(hwnd, kIdObjectiveCombo);
        HWND objective_name_edit = GetDlgItem(hwnd, kIdObjectiveNameEdit);
        HWND calibration_unit = GetDlgItem(hwnd, kIdCalibrationUnitCombo);
        HWND pseudo_color_combo = GetDlgItem(hwnd, kIdPseudoColorCombo);
        HWND histogram_channel_combo = GetDlgItem(hwnd, kIdHistogramChannelCombo);
        HWND dye_combo = GetDlgItem(hwnd, kIdDyeCombo);
        HWND dye_name_edit = GetDlgItem(hwnd, kIdDyeNameEdit);
        HWND dye_excitation_edit = GetDlgItem(hwnd, kIdDyeExcitationEdit);
        HWND dye_emission_edit = GetDlgItem(hwnd, kIdDyeEmissionEdit);
        HWND dye_red_edit = GetDlgItem(hwnd, kIdDyeRedEdit);
        HWND dye_green_edit = GetDlgItem(hwnd, kIdDyeGreenEdit);
        HWND dye_blue_edit = GetDlgItem(hwnd, kIdDyeBlueEdit);
        for (MeasurementUnit unit : CalibrationProfile::CalibrationUnitOptions()) {
            const std::wstring label_text = CalibrationProfile::UnitLabel(unit);
            SendMessageW(calibration_unit, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label_text.c_str()));
        }
        SendMessageW(calibration_unit, CB_SETCURSEL, 0, 0);
        app->InitializeObjectiveControls(objective_combo, objective_name_edit);

        for (const std::wstring& label_text : PreviewDisplayActions::PseudoColorLabels()) {
            SendMessageW(pseudo_color_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label_text.c_str()));
        }
        SendMessageW(pseudo_color_combo, CB_SETCURSEL, 0, 0);

        for (int ch = 0; ch < 4; ++ch) {
            SendMessageW(histogram_channel_combo, CB_ADDSTRING, 0,
                         reinterpret_cast<LPARAM>(HistogramChannelLabel(static_cast<HistogramChannel>(ch))));
        }
        SendMessageW(histogram_channel_combo, CB_SETCURSEL, 0, 0);

        // Brightness trackbar: range 0-200 maps to -100..+100
        HWND brightness_slider = GetDlgItem(hwnd, kIdHistogramBrightnessSlider);
        SendMessageW(brightness_slider, TBM_SETRANGE, TRUE, MAKELONG(0, 200));
        SendMessageW(brightness_slider, TBM_SETPOS, TRUE, 100);
        SendMessageW(brightness_slider, TBM_SETTICFREQ, 25, 0);

        // Contrast trackbar: range 0-200 maps to -100..+100
        HWND contrast_slider = GetDlgItem(hwnd, kIdHistogramContrastSlider);
        SendMessageW(contrast_slider, TBM_SETRANGE, TRUE, MAKELONG(0, 200));
        SendMessageW(contrast_slider, TBM_SETPOS, TRUE, 100);
        SendMessageW(contrast_slider, TBM_SETTICFREQ, 25, 0);

        // Gamma trackbar: range 1-30 maps to 0.1..3.0
        HWND gamma_slider = GetDlgItem(hwnd, kIdHistogramGammaSlider);
        SendMessageW(gamma_slider, TBM_SETRANGE, TRUE, MAKELONG(1, 30));
        SendMessageW(gamma_slider, TBM_SETPOS, TRUE, 10);
        SendMessageW(gamma_slider, TBM_SETTICFREQ, 5, 0);

        // Window Level trackbar: range 0-255, default 128
        HWND wl_slider = GetDlgItem(hwnd, kIdHistogramWindowLevelSlider);
        SendMessageW(wl_slider, TBM_SETRANGE, TRUE, MAKELONG(0, 255));
        SendMessageW(wl_slider, TBM_SETPOS, TRUE, 128);
        SendMessageW(wl_slider, TBM_SETTICFREQ, 32, 0);

        // Window Width trackbar: range 1-256, default 256
        HWND ww_slider = GetDlgItem(hwnd, kIdHistogramWindowWidthSlider);
        SendMessageW(ww_slider, TBM_SETRANGE, TRUE, MAKELONG(1, 256));
        SendMessageW(ww_slider, TBM_SETPOS, TRUE, 256);
        SendMessageW(ww_slider, TBM_SETTICFREQ, 32, 0);

        app->SyncPanelCardButtons();
        app->SyncFunctionPanelChrome();
        app->InitializeDyeCombo(dye_combo);
        app->SyncSelectedDyeControls(
            dye_combo,
            dye_name_edit,
            dye_excitation_edit,
            dye_emission_edit,
            dye_red_edit,
            dye_green_edit,
            dye_blue_edit);
        app->InitializeStitchControls();

        LayoutControls(hwnd);
        if (app->RefreshCameraList(device_combo)) {
            app->Start();
        }
        return 0;
    }
    case WM_SIZE:
        LayoutControls(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_DRAWITEM: {
        CameraPreviewApp* app = GetApp(hwnd);
        const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lparam);
        if (app && item && app->DrawPanelCategoryButton(*item)) {
            return TRUE;
        }
        if (app && item && app->DrawToolbarButton(*item)) {
            return TRUE;
        }
        break;
    }
    case WM_HSCROLL: {
        CameraPreviewApp* app = GetApp(hwnd);
        if (app) {
            app->OnHistogramSlider(hwnd, lparam);
        }
        return 0;
    }
    case WM_VSCROLL: {
        CameraPreviewApp* app = GetApp(hwnd);
        if (app && reinterpret_cast<HWND>(lparam) == GetDlgItem(hwnd, kIdPanelScrollBar)) {
            app->HandlePanelScrollCommand(LOWORD(wparam));
            return 0;
        }
        break;
    }
    case WM_COMMAND: {
        CameraPreviewApp* app = GetApp(hwnd);
        if (!app) {
            break;
        }
        const int panel_category = WindowControlLayout::PanelCategoryFromCardControl(LOWORD(wparam));
        if (panel_category >= 0) {
            app->ShowPanelCategory(panel_category);
            LayoutControls(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (LOWORD(wparam) == kIdDeviceCombo && HIWORD(wparam) == CBN_SELCHANGE) {
            app->UpdateSelectedCamera(GetDlgItem(hwnd, kIdDeviceCombo));
            return 0;
        }
        if (LOWORD(wparam) == kIdPseudoColorCombo && HIWORD(wparam) == CBN_SELCHANGE) {
            app->UpdatePseudoColor(GetDlgItem(hwnd, kIdPseudoColorCombo));
            return 0;
        }
        if (LOWORD(wparam) == kIdHistogramChannelCombo && HIWORD(wparam) == CBN_SELCHANGE) {
            app->UpdateHistogramChannel(GetDlgItem(hwnd, kIdHistogramChannelCombo));
            return 0;
        }
        if (LOWORD(wparam) == kIdHistogramResetAdjust && HIWORD(wparam) == BN_CLICKED) {
            app->ResetImageAdjust(hwnd);
            return 0;
        }
        if (LOWORD(wparam) == kIdObjectiveCombo && HIWORD(wparam) == CBN_SELCHANGE) {
            app->SelectObjective(GetDlgItem(hwnd, kIdObjectiveCombo), GetDlgItem(hwnd, kIdObjectiveNameEdit));
            return 0;
        }
        if (LOWORD(wparam) == kIdDyeCombo && HIWORD(wparam) == CBN_SELCHANGE) {
            app->SyncSelectedDyeControls(
                GetDlgItem(hwnd, kIdDyeCombo),
                GetDlgItem(hwnd, kIdDyeNameEdit),
                GetDlgItem(hwnd, kIdDyeExcitationEdit),
                GetDlgItem(hwnd, kIdDyeEmissionEdit),
                GetDlgItem(hwnd, kIdDyeRedEdit),
                GetDlgItem(hwnd, kIdDyeGreenEdit),
                GetDlgItem(hwnd, kIdDyeBlueEdit));
            return 0;
        }
        if (LOWORD(wparam) == kIdResultsList && HIWORD(wparam) == LBN_SELCHANGE) {
            app->SyncSelectedMeasurementName(
                GetDlgItem(hwnd, kIdResultsList),
                GetDlgItem(hwnd, kIdMeasurementNameEdit));
            return 0;
        }
        if (LOWORD(wparam) == kIdChannelList && HIWORD(wparam) == LBN_SELCHANGE) {
            app->SyncSelectedChannelControls(
                GetDlgItem(hwnd, kIdChannelList),
                GetDlgItem(hwnd, kIdChannelVisible),
                GetDlgItem(hwnd, kIdChannelBlackEdit),
                GetDlgItem(hwnd, kIdChannelWhiteEdit));
            return 0;
        }        if ((LOWORD(wparam) == kIdStitchModeCombo ||
             LOWORD(wparam) == kIdStitchMethodCombo ||
             LOWORD(wparam) == kIdStitchTransformCombo ||
             LOWORD(wparam) == kIdStitchBlendCombo) &&
            HIWORD(wparam) == CBN_SELCHANGE) {
            app->UpdateStitchSettingsFromControls();
            return 0;
        }
        switch (LOWORD(wparam)) {
        case kIdToggleFunctionPanel:
            app->ToggleFunctionPanel();
            return 0;
        case kIdTogglePanelDock:
            app->ToggleFunctionPanelDock();
            return 0;
        case kIdDockFunctionPanelLeft:
            app->SetFunctionPanelDockedLeft(true);
            return 0;
        case kIdDockFunctionPanelRight:
            app->SetFunctionPanelDockedLeft(false);
            return 0;
        case kIdLanguageEnglish:
            app->ReloadUILanguage(UILanguage::English);
            return 0;
        case kIdLanguageChinese:
            app->ReloadUILanguage(UILanguage::Chinese);
            return 0;
        case kIdAbout:
            app->ShowAboutDialog();
            return 0;
        case kIdExit:
            DestroyWindow(hwnd);
            return 0;
        case kIdOpen:
            app->UpdateSelectedCamera(GetDlgItem(hwnd, kIdDeviceCombo));
            app->Start();
            return 0;
        case kIdStop:
            app->Stop();
            InvalidatePreview(hwnd);
            return 0;
        case kIdFitView:
            app->FitView();
            return 0;
        case kIdRefreshDevices:
            app->RefreshCameraList(GetDlgItem(hwnd, kIdDeviceCombo));
            InvalidatePreview(hwnd);
            InvalidateStatus(hwnd);
            return 0;
        case kIdExposureApply:
            app->ApplyExposure(GetDlgItem(hwnd, kIdExposureEdit));
            return 0;
        case kIdAutoExposure:
            app->ApplyAutoExposure();
            return 0;
        case kIdCameraExposureApply:
            app->ApplyExposure(GetDlgItem(hwnd, kIdCameraExposureEdit));
            return 0;
        case kIdCameraGainApply:
            app->ApplyGain(GetDlgItem(hwnd, kIdCameraGainEdit));
            return 0;
        case kIdWhiteBalance:
            app->ApplyWhiteBalance();
            return 0;
        case kIdCalibrate:
            app->BeginCalibration(GetDlgItem(hwnd, kIdCalibrationLengthEdit), GetDlgItem(hwnd, kIdCalibrationUnitCombo));
            return 0;
        case kIdClearCalibration:
            app->ClearCalibration();
            return 0;
        case kIdAddObjective:
            app->AddObjective(GetDlgItem(hwnd, kIdObjectiveCombo), GetDlgItem(hwnd, kIdObjectiveNameEdit));
            return 0;
        case kIdRenameObjective:
            app->RenameSelectedObjective(GetDlgItem(hwnd, kIdObjectiveCombo), GetDlgItem(hwnd, kIdObjectiveNameEdit));
            return 0;
        case kIdDeleteObjective:
            app->DeleteSelectedObjective(GetDlgItem(hwnd, kIdObjectiveCombo), GetDlgItem(hwnd, kIdObjectiveNameEdit));
            return 0;
        case kIdLengthTool:
            app->BeginLengthMeasurement();
            return 0;
        case kIdAngleTool:
            app->BeginAngleMeasurement();
            return 0;
        case kIdRectangleAreaTool:
            app->BeginRectangleAreaMeasurement();
            return 0;
        case kIdPolygonAreaTool:
            app->BeginPolygonAreaMeasurement();
            return 0;
        case kIdFinishPolygonArea:
            app->FinishPolygonAreaMeasurement();
            return 0;
        case kIdDeleteMeasurement:
            app->DeleteSelectedMeasurement(GetDlgItem(hwnd, kIdResultsList));
            return 0;
        case kIdRenameMeasurement:
            app->RenameSelectedMeasurement(
                GetDlgItem(hwnd, kIdResultsList),
                GetDlgItem(hwnd, kIdMeasurementNameEdit));
            return 0;
        case kIdClearMeasurements:
            app->ClearMeasurements(GetDlgItem(hwnd, kIdResultsList));
            return 0;
        case kIdExportCsv:
            app->ExportMeasurementsCsv();
            return 0;
        case kIdExportImage:
            app->ExportImage();
            return 0;
        case kIdOpenImage:
            app->OpenImage();
            return 0;
        case kIdSaveDiagnostics:
            app->SaveDiagnosticsReport();
            return 0;
        case kIdDesignReportTemplate:
            app->ShowReportTemplateDesigner();
            return 0;
        case kIdLoadReportTemplate:
            app->LoadReportTemplate();
            return 0;
        case kIdClearReportTemplate:
            app->ClearReportTemplate();
            return 0;
        case kIdSaveDye:
            app->SaveDyeProfile(
                GetDlgItem(hwnd, kIdDyeCombo),
                GetDlgItem(hwnd, kIdDyeNameEdit),
                GetDlgItem(hwnd, kIdDyeExcitationEdit),
                GetDlgItem(hwnd, kIdDyeEmissionEdit),
                GetDlgItem(hwnd, kIdDyeRedEdit),
                GetDlgItem(hwnd, kIdDyeGreenEdit),
                GetDlgItem(hwnd, kIdDyeBlueEdit));
            return 0;
        case kIdDeleteDye:
            app->DeleteSelectedDye(
                GetDlgItem(hwnd, kIdDyeCombo),
                GetDlgItem(hwnd, kIdDyeNameEdit),
                GetDlgItem(hwnd, kIdDyeExcitationEdit),
                GetDlgItem(hwnd, kIdDyeEmissionEdit),
                GetDlgItem(hwnd, kIdDyeRedEdit),
                GetDlgItem(hwnd, kIdDyeGreenEdit),
                GetDlgItem(hwnd, kIdDyeBlueEdit));
            return 0;
        case kIdAddChannel:
            app->AddCurrentFrameAsChannel(
                GetDlgItem(hwnd, kIdDyeCombo),
                GetDlgItem(hwnd, kIdChannelList),
                GetDlgItem(hwnd, kIdFusionPreview));
            return 0;
        case kIdFusionPreview:
            app->UpdateFusionPreview(GetDlgItem(hwnd, kIdFusionPreview));
            return 0;
        case kIdClearChannels:
            app->ClearFluorescenceChannels(
                GetDlgItem(hwnd, kIdChannelList),
                GetDlgItem(hwnd, kIdFusionPreview));
            return 0;
        case kIdChannelVisible:
        case kIdApplyChannel:
            app->ApplySelectedChannelSettings(
                GetDlgItem(hwnd, kIdChannelList),
                GetDlgItem(hwnd, kIdChannelVisible),
                GetDlgItem(hwnd, kIdChannelBlackEdit),
                GetDlgItem(hwnd, kIdChannelWhiteEdit));
            return 0;
        case kIdSelectStitchDirectory:
            app->SelectStitchDirectory();
            return 0;
        case kIdSelectStitchFiles:
            app->SelectStitchFiles();
            return 0;
        case kIdAddStitchTile:
            app->AddCurrentFrameAsStitchTile();
            return 0;
        case kIdStartLiveStitch:
            app->StartLiveStitchCapture();
            return 0;
        case kIdStopLiveStitch:
            app->StopLiveStitchCapture();
            return 0;
        case kIdBuildStitch:
            app->BuildStitchPreview();
            return 0;
        case kIdSaveStitchResult:
            app->SaveStitchResult();
            return 0;
        case kIdStitchOrbRegistration:
            app->UpdateStitchRegistrationMode();
            return 0;
        case kIdDeleteStitchTile:
            app->DeleteSelectedStitchTile();
            return 0;
        case kIdClearStitchTiles:
            app->ClearStitchTiles();
            return 0;
        case kIdAddEdfFrame:
            app->AddCurrentFrameAsEdfFrame();
            return 0;
        case kIdBuildEdf:
            app->BuildEdfPreview();
            return 0;
        case kIdShowEdfComposite:
            app->ShowEdfCompositeFrame();
            return 0;
        case kIdShowEdfFocusMap:
            app->ShowEdfFocusMap();
            return 0;
        case kIdRetryProcessing:
            app->RetryProcessing();
            return 0;
        case kIdClearProcessing:
            app->ClearProcessing();
            return 0;
        case kIdOpenProject:
            app->OpenProject();
            return 0;
        case kIdSaveProject:
            app->SaveProject();
            return 0;
        default:
            break;
        }
        break;
    }
    case WM_SETCURSOR: {
        CameraPreviewApp* app = GetApp(hwnd);
        if (app && LOWORD(lparam) == HTCLIENT) {
            POINT point = {};
            GetCursorPos(&point);
            ScreenToClient(hwnd, &point);
            if (app->ShouldShowFunctionPanelDockCursor(point)) {
                SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
                return TRUE;
            }
        }
        break;
    }
    case WM_DROPFILES: {
        HDROP drop = reinterpret_cast<HDROP>(wparam);
        CameraPreviewApp* app = GetApp(hwnd);
        const UINT file_count = DragQueryFileW(drop, 0xFFFFFFFFU, nullptr, 0);
        if (app && file_count > 0) {
            std::vector<std::wstring> paths;
            paths.reserve(file_count);
            for (UINT index = 0; index < file_count; ++index) {
                const UINT path_length = DragQueryFileW(drop, index, nullptr, 0);
                if (path_length == 0) {
                    continue;
                }
                std::wstring path(path_length + 1U, L'\0');
                if (DragQueryFileW(drop, index, path.data(), path_length + 1U) > 0) {
                    path.resize(path_length);
                    paths.push_back(std::move(path));
                }
            }
            app->OpenDroppedFiles(paths);
        }
        DragFinish(drop);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        CameraPreviewApp* app = GetApp(hwnd);
        if (app) {
            POINT point = {
                static_cast<int>(static_cast<short>(LOWORD(lparam))),
                static_cast<int>(static_cast<short>(HIWORD(lparam)))
            };
            if (app->BeginFunctionPanelResize(point)) {
                return 0;
            }
            if (app->BeginFunctionPanelDockDrag(point)) {
                return 0;
            }
            if (app->BeginMeasurementEdit(point, GetDlgItem(hwnd, kIdResultsList))) {
                return 0;
            }
            if (app->HandleLeftClick(point)) {
                return 0;
            }
        }
        break;
    }
    case WM_MOUSEWHEEL: {
        CameraPreviewApp* app = GetApp(hwnd);
        if (app) {
            POINT point = {
                static_cast<int>(static_cast<short>(LOWORD(lparam))),
                static_cast<int>(static_cast<short>(HIWORD(lparam)))
            };
            if (app->HandleMouseWheel(point, static_cast<short>(HIWORD(wparam)))) {
                return 0;
            }
        }
        break;
    }
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN: {
        CameraPreviewApp* app = GetApp(hwnd);
        if (app) {
            POINT point = {
                static_cast<int>(static_cast<short>(LOWORD(lparam))),
                static_cast<int>(static_cast<short>(HIWORD(lparam)))
            };
            if (app->BeginPan(point)) {
                return 0;
            }
        }
        break;
    }
    case WM_MOUSEMOVE: {
        CameraPreviewApp* app = GetApp(hwnd);
        if (app) {
            POINT point = {
                static_cast<int>(static_cast<short>(LOWORD(lparam))),
                static_cast<int>(static_cast<short>(HIWORD(lparam)))
            };
            if (app->ContinueFunctionPanelResize(point)) {
                return 0;
            }
            if (app->ContinueFunctionPanelDockDrag(point)) {
                return 0;
            }
            if (app->ContinueMeasurementEdit(point)) {
                return 0;
            }
            if (app->ContinuePan(point)) {
                return 0;
            }
        }
        break;
    }
    case WM_LBUTTONUP: {
        CameraPreviewApp* app = GetApp(hwnd);
        if (app) {
            if (app->EndFunctionPanelResize()) {
                return 0;
            }
            if (app->EndFunctionPanelDockDrag()) {
                return 0;
            }
            app->EndMeasurementEdit();
            return 0;
        }
        break;
    }
    case WM_RBUTTONUP:
    case WM_MBUTTONUP: {
        CameraPreviewApp* app = GetApp(hwnd);
        if (app) {
            app->EndPan();
            return 0;
        }
        break;
    }
    case WM_CAPTURECHANGED: {
        CameraPreviewApp* app = GetApp(hwnd);
        if (app) {
            app->EndFunctionPanelResize();
            app->EndFunctionPanelDockDrag();
            app->EndMeasurementEdit();
            app->EndPan();
        }
        break;
    }
    case kMsgFrameReady:
        if (CameraPreviewApp* app = GetApp(hwnd)) {
            app->HandleFrameReady();
        } else {
            InvalidatePreview(hwnd);
        }
        return 0;
    case kMsgStatusChanged:
        InvalidateStatus(hwnd);
        return 0;
    case kMsgProcessingFinished: {
        CameraPreviewApp* app = GetApp(hwnd);
        if (app) {
            app->ApplyProcessingResult();
        }
        return 0;
    }
    case kMsgLiveStitchPreviewReady: {
        CameraPreviewApp* app = GetApp(hwnd);
        if (app) {
            app->ApplyLiveStitchPreviewResult();
        }
        return 0;
    }
    case kMsgLiveStitchCaptureReady: {
        CameraPreviewApp* app = GetApp(hwnd);
        if (app) {
            app->ApplyLiveStitchCaptureResult();
        }
        return 0;
    }
    case WM_TIMER:
        if (wparam == kLiveStitchTimerId) {
            if (CameraPreviewApp* app = GetApp(hwnd)) {
                app->CaptureLiveStitchTick();
            }
            return 0;
        }
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint = {};
        HDC hdc = BeginPaint(hwnd, &paint);
        CameraPreviewApp* app = GetApp(hwnd);
        if (app) {
            app->PaintToWindow(hdc, paint.rcPaint);
        }
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_DESTROY: {
        DragAcceptFiles(hwnd, FALSE);
        CameraPreviewApp* app = GetApp(hwnd);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        delete app;
        PostQuitMessage(0);
        return 0;
    }
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command)
{
    InitCommonControls();

    const wchar_t* class_name = L"MUCamCameraViewWindow";

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = class_name;

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(nullptr, L"Failed to register window class.", L"CameraView", MB_ICONERROR | MB_OK);
        return 1;
    }

    HMENU main_menu = CreateMainMenu(UILanguage::English);
    HWND hwnd = CreateWindowExW(
        0,
        class_name,
        L"CameraView - MUCam Preview",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1100,
        760,
        nullptr,
        main_menu,
        instance,
        nullptr);

    if (!hwnd) {
        DestroyMenu(main_menu);
        MessageBoxW(nullptr, L"Failed to create main window.", L"CameraView", MB_ICONERROR | MB_OK);
        return 1;
    }

    SetLanguageProperty(hwnd, UILanguage::English);
    ShowWindow(hwnd, show_command);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
