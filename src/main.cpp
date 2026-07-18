#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr UINT kMsgFrameReady = WM_APP + 1;
constexpr UINT kMsgStatusChanged = WM_APP + 2;
constexpr UINT kMsgProcessingFinished = WM_APP + 3;
constexpr int kPanelTitleHeight = 34;
constexpr const wchar_t* kFunctionPanelVisibleProperty = L"CameraViewFunctionPanelVisible";
constexpr const wchar_t* kFunctionPanelDockLeftProperty = L"CameraViewFunctionPanelDockLeft";
constexpr INT_PTR kFunctionPanelVisibleValue = 1;
constexpr INT_PTR kFunctionPanelHiddenValue = 2;
constexpr INT_PTR kFunctionPanelDockLeftValue = 1;
constexpr INT_PTR kFunctionPanelDockRightValue = 2;

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

HMENU CreateMainMenu()
{
    HMENU menu = CreateMenu();
    HMENU file_menu = CreatePopupMenu();
    HMENU view_menu = CreatePopupMenu();
    HMENU camera_menu = CreatePopupMenu();
    HMENU processing_menu = CreatePopupMenu();
    HMENU measurement_menu = CreatePopupMenu();

    AppendMenuW(file_menu, MF_STRING, kIdOpenImage, L"Open Image...");
    AppendMenuW(file_menu, MF_STRING, kIdExportImage, L"Export Image...");
    AppendMenuW(file_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file_menu, MF_STRING, kIdOpenProject, L"Open Project...");
    AppendMenuW(file_menu, MF_STRING, kIdSaveProject, L"Save Project...");
    AppendMenuW(file_menu, MF_STRING, kIdSaveDiagnostics, L"Save Diagnostic...");
    AppendMenuW(file_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file_menu, MF_STRING, kIdExit, L"Exit");

    AppendMenuW(view_menu, MF_STRING | MF_CHECKED, kIdToggleFunctionPanel, L"Function Panel");
    AppendMenuW(view_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(view_menu, MF_STRING | MF_CHECKED, kIdDockFunctionPanelLeft, L"Dock Left");
    AppendMenuW(view_menu, MF_STRING, kIdDockFunctionPanelRight, L"Dock Right");
    AppendMenuW(view_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(view_menu, MF_STRING, kIdFitView, L"Fit Image");

    AppendMenuW(camera_menu, MF_STRING, kIdRefreshDevices, L"Refresh Devices");
    AppendMenuW(camera_menu, MF_STRING, kIdOpen, L"Open Camera");
    AppendMenuW(camera_menu, MF_STRING, kIdStop, L"Stop Camera");
    AppendMenuW(camera_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(camera_menu, MF_STRING, kIdAutoExposure, L"Auto Exposure");
    AppendMenuW(camera_menu, MF_STRING, kIdWhiteBalance, L"White Balance");

    AppendMenuW(processing_menu, MF_STRING, kIdAddStitchTile, L"Add Tile");
    AppendMenuW(processing_menu, MF_STRING, kIdBuildStitch, L"Stitch");
    AppendMenuW(processing_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(processing_menu, MF_STRING, kIdAddEdfFrame, L"Add EDF Frame");
    AppendMenuW(processing_menu, MF_STRING, kIdBuildEdf, L"Build EDF");
    AppendMenuW(processing_menu, MF_STRING, kIdClearProcessing, L"Clear Processing");

    AppendMenuW(measurement_menu, MF_STRING, kIdCalibrate, L"Calibrate");
    AppendMenuW(measurement_menu, MF_STRING, kIdLengthTool, L"Length");
    AppendMenuW(measurement_menu, MF_STRING, kIdAngleTool, L"Angle");
    AppendMenuW(measurement_menu, MF_STRING, kIdRectangleAreaTool, L"Rectangle Area");
    AppendMenuW(measurement_menu, MF_STRING, kIdPolygonAreaTool, L"Polygon Area");
    AppendMenuW(measurement_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(measurement_menu, MF_STRING, kIdExportCsv, L"Export CSV...");

    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file_menu), L"File");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(view_menu), L"View");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(camera_menu), L"Camera");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(processing_menu), L"Processing");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(measurement_menu), L"Measurement");
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
    DrawMenuBar(hwnd);
}

RECT GetPreviewRect(HWND hwnd)
{
    RECT rect = {};
    GetClientRect(hwnd, &rect);
    return WindowLayout::PreviewRect(
        rect,
        IsFunctionPanelVisible(hwnd),
        IsFunctionPanelDockedLeft(hwnd));
}

RECT GetSidePanelRect(HWND hwnd)
{
    RECT rect = {};
    GetClientRect(hwnd, &rect);
    return WindowLayout::SidePanelRect(
        rect,
        IsFunctionPanelVisible(hwnd),
        IsFunctionPanelDockedLeft(hwnd));
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

} // namespace

void LayoutControls(HWND hwnd, bool repaint_children = true);

class CameraPreviewApp {
public:
    explicit CameraPreviewApp(HWND hwnd) : hwnd_(hwnd)
    {
        SetFunctionPanelVisibleProperty(hwnd_, function_panel_visible_);
        SetFunctionPanelDockLeftProperty(hwnd_, function_panel_docked_left_);
    }
    ~CameraPreviewApp()
    {
        Stop();
        RequestProcessingCancel();
        WaitForProcessingWorker();
        ReleasePaintBuffer();
        RemoveFunctionPanelVisibleProperty(hwnd_);
        RemoveFunctionPanelDockLeftProperty(hwnd_);
    }

    void Start()
    {
        Stop();
        ClearLatestFrame();
        if (selected_camera_index_ < 0) {
            SetStatus(CameraControlStatusFormatter::FormatNoCameraSelectedForStart());
            return;
        }
        SetStatus(CameraControlStatusFormatter::FormatOpeningCamera());
        running_ = true;
        worker_ = std::thread(&CameraPreviewApp::CaptureThread, this);
    }

    void Stop()
    {
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
            SetStatus(L"Open camera before auto exposure.");
            return;
        }
        if (!camera_driver_.HasAutoExposureControl() || !camera_driver_.ApplyAutoExposure()) {
            SetStatus(L"Auto exposure is not available for this camera.");
            return;
        }
        SetStatus(L"Auto exposure applied.");
    }

    void ApplyGain(HWND edit)
    {
        double gain = 0.0;
        if (!ReadPositiveNumber(edit, gain)) {
            SetStatus(L"Gain must be a positive number.");
            return;
        }
        if (!camera_driver_.IsOpen()) {
            SetStatus(L"Open camera before setting gain.");
            return;
        }
        if (!camera_driver_.HasGainControl() || !camera_driver_.SetGain(static_cast<float>(gain))) {
            SetStatus(L"Gain control is not available for this camera.");
            return;
        }
        SetStatus(L"Gain set.");
    }

    void ApplyWhiteBalance()
    {
        if (!camera_driver_.IsOpen()) {
            SetStatus(L"Open camera before white balance.");
            return;
        }
        if (!camera_driver_.HasWhiteBalanceControl() || !camera_driver_.ApplyWhiteBalance()) {
            SetStatus(L"White balance is not available for this camera.");
            return;
        }
        SetStatus(L"White balance applied.");
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

    void SyncPanelCardButtons() const
    {
        const int card_ids[] = {
            kIdCameraPanelCard,
            kIdImagePanelCard,
            kIdFluorescencePanelCard,
            kIdProcessingPanelCard,
            kIdMeasurementPanelCard,
            kIdProjectPanelCard
        };
        const std::vector<std::wstring>& labels = WindowControlLayout::PanelCategoryLabels();
        for (std::size_t i = 0; i < labels.size(); ++i) {
            HWND button = GetDlgItem(hwnd_, card_ids[i]);
            if (!button) {
                continue;
            }
            SetWindowTextW(button, labels[i].c_str());
            InvalidateRect(button, nullptr, TRUE);
        }
    }

    void ShowPanelCategory(int panel_category)
    {
        panel_category_ = WindowControlLayout::NormalizePanelCategory(panel_category);
        panel_scroll_offset_ = 0;
        SyncPanelCardButtons();
        SetStatus(L"Function card: " + WindowControlLayout::PanelCategoryLabels()[static_cast<std::size_t>(panel_category_)] + L".");
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

    void SyncFunctionPanelChrome() const
    {
        HWND toggle_button = GetDlgItem(hwnd_, kIdToggleFunctionPanel);
        if (toggle_button) {
            SetWindowTextW(toggle_button, function_panel_visible_ ? L"Hide Panel" : L"Show Panel");
        }
        HWND dock_button = GetDlgItem(hwnd_, kIdTogglePanelDock);
        if (dock_button) {
            SetWindowTextW(dock_button, function_panel_docked_left_ ? L"Dock Right" : L"Dock Left");
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
        SetStatus(function_panel_visible_ ? L"Function panel shown." : L"Function panel hidden.");
    }

    void ToggleFunctionPanel()
    {
        SetFunctionPanelVisible(!function_panel_visible_);
    }

    void SetFunctionPanelDockedLeft(bool dock_left)
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
        SetStatus(function_panel_docked_left_ ? L"Function panel docked left." : L"Function panel docked right.");
    }

    void ToggleFunctionPanelDock()
    {
        SetFunctionPanelDockedLeft(!function_panel_docked_left_);
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
                function_panel_docked_left_));
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
                function_panel_docked_left_);
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
                function_panel_docked_left_));
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
            SetStatus(L"Channel range must be 0-255.");
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
            ProcessingQueueActions::AddStitchTile(
                stitch_tiles_,
                processing_frames_,
                std::move(frame),
                ReadEditText(GetDlgItem(hwnd_, kIdStitchSearchEdit), 32),
                stitch_search_percent_);
        const auto add_tile_elapsed = std::chrono::steady_clock::now() - add_tile_start;
        const long long add_tile_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(add_tile_elapsed).count();
        stitch_search_percent_ = result.stitch_search_percent;
        if (result.preview_changed) {
            InvalidatePreviewFrameCache();
            InvalidatePreview(hwnd_);
        }
        if (result.changed) {
            HWND stitch_tile_list = GetDlgItem(hwnd_, kIdStitchTileList);
            AppendStitchTileListItem(stitch_tile_list);
            if (stitch_tile_list && result.stitch_tile_count > 0) {
                SendMessageW(stitch_tile_list, LB_SETCURSEL, result.stitch_tile_count - 1, 0);
            }
        }
        SetStatus(L"Add Tile " + std::to_wstring(add_tile_ms) + L" ms. " + result.message);
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

        processing_frames_.Clear();
        RefreshStitchTileList(list);
        if (list && result.next_selection) {
            SendMessageW(list, LB_SETCURSEL, static_cast<WPARAM>(*result.next_selection), 0);
        }
        InvalidatePreviewFrameCache();
        InvalidatePreview(hwnd_);
        SetStatus(result.message);
    }

    void ClearStitchTiles()
    {
        const StitchTileListActionResult result = StitchTileListActions::Clear(stitch_tiles_);
        if (!result.changed) {
            SetStatus(result.message);
            return;
        }

        processing_frames_.Clear();
        RefreshStitchTileList(GetDlgItem(hwnd_, kIdStitchTileList));
        InvalidatePreviewFrameCache();
        InvalidatePreview(hwnd_);
        SetStatus(result.message);
    }

    void BuildStitchPreview()
    {
        ProcessingBuildActionResult result =
            ProcessingBuildInputActions::PrepareStitch(
                stitch_tiles_,
                ReadEditText(GetDlgItem(hwnd_, kIdStitchSearchEdit), 32));
        if (!result.can_start) {
            SetStatus(result.message);
            return;
        }
        stitch_search_percent_ = result.stitch_search_percent;
        StartStitchProcessing(std::move(result.stitch_tiles), result.stitch_search_percent, true);
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
            if (StartStitchProcessing(
                    std::move(result.request.stitch_tiles),
                    result.request.stitch_search_percent,
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

    void Paint(HDC hdc)
    {
        RECT client = {};
        GetClientRect(hwnd_, &client);

        RECT toolbar = client;
        toolbar.bottom = std::min(toolbar.bottom, static_cast<LONG>(WindowLayout::ToolbarHeight()));
        FillSolidRect(hdc, toolbar, RGB(245, 247, 250));

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
            const std::vector<std::wstring>& labels = WindowControlLayout::PanelCategoryLabels();
            const std::wstring panel_label =
                L"Functions - " + labels[static_cast<std::size_t>(WindowControlLayout::NormalizePanelCategory(panel_category_))];
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

        const ImageFrame& display_frame = CurrentPreviewFrame();
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
                function_panel_docked_left_);
        if (max_scroll <= 0) {
            return ScrollPanelTo(0);
        }

        const int page_size =
            std::max(
                54,
                WindowControlLayout::PanelScrollPage(
                    client,
                    function_panel_visible_,
                    function_panel_docked_left_) - 24);
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
                function_panel_docked_left_);
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
            SetStatus(L"No image frame to fit.");
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
        SetStatus(L"Measurements cleared.");
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
        SetStatus(L"Drag to edit measurement point.");
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
        SetStatus(L"Measurement point updated.");
    }

    void ExportMeasurementsCsv()
    {
        if (measurements_.Empty()) {
            SetStatus(L"No measurements to export.");
            return;
        }

        std::wstring file_name;
        if (!FileDialog::SaveCsv(hwnd_, file_name)) {
            SetStatus(L"CSV export canceled.");
            return;
        }

        const ExportActionResult result = ExportActions::SaveMeasurementsCsv(
                std::filesystem::path(file_name),
                measurements_,
                calibration_,
                DisplayUnit());
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
            SetStatus(L"No image frame to export.");
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
                display_mode);
        SetStatus(result.message);
    }

    void OpenImage()
    {
        std::wstring file_name;
        if (!FileDialog::OpenImage(hwnd_, file_name)) {
            SetStatus(L"Image open canceled.");
            return;
        }

        ImageFrame loaded;
        std::wstring error;
        if (!ImageExporter::LoadRasterImage(std::filesystem::path(file_name), loaded, error)) {
            SetStatus(error.empty() ? L"Failed to open image." : error);
            return;
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
        SetStatus(L"Image opened: " + image_size + L".");
    }

    void SaveDiagnosticsReport()
    {
        std::wstring file_name;
        if (!FileDialog::SaveText(hwnd_, file_name)) {
            SetStatus(L"Diagnostic save canceled.");
            return;
        }

        const ExportActionResult result =
            ExportActions::SaveDiagnosticReport(std::filesystem::path(file_name), BuildDiagnosticsReport());
        SetStatus(result.message);
    }

    void SaveProject()
    {
        std::wstring file_name;
        if (!FileDialog::SaveProject(hwnd_, file_name)) {
            SetStatus(L"Project save canceled.");
            return;
        }

        const ProjectActionResult result = ProjectActions::SaveProject(
            std::filesystem::path(file_name),
            calibration_,
            measurements_,
            dye_library_,
            fluorescence_channels_,
            edf_options_,
            stitch_search_percent_);
        SetStatus(result.message);
    }

    void OpenProject()
    {
        std::wstring file_name;
        if (!FileDialog::OpenProject(hwnd_, file_name)) {
            SetStatus(L"Project open canceled.");
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
                show_fusion_preview_,
                processing_retry_,
                processing_frames_
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
        if (HWND stitch_search_edit = GetDlgItem(hwnd_, kIdStitchSearchEdit)) {
            const std::wstring search_text = std::to_wstring(stitch_search_percent_);
            SetWindowTextW(stitch_search_edit, search_text.c_str());
        }
        RefreshMeasurementList(GetDlgItem(hwnd_, kIdResultsList));
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

    bool StartStitchProcessing(std::vector<StitchTile> tiles, int search_percent, bool remember_snapshot)
    {
        const ProcessingStartActionResult start = ProcessingStartActions::StartStitch(
            processing_state_,
            processing_retry_,
            tiles,
            search_percent,
            remember_snapshot,
            [this]() { WaitForProcessingWorker(); });
        if (!start.can_start) {
            SetStatus(start.message);
            return false;
        }

        processing_worker_ = ProcessingWorkerActions::StartStitch(
            start.launch,
            std::move(tiles),
            search_percent,
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
            ExportActions::SaveImage(path, frame, MeasurementCollection(), display_mode);
        if (!save_result.saved) {
            return L"Auto-save failed: " + save_result.message;
        }

        std::error_code absolute_error;
        std::filesystem::path absolute_path = std::filesystem::absolute(path, absolute_error);
        if (absolute_error) {
            absolute_path = path;
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

    static bool ReadPositiveNumber(HWND edit, double& value)
    {
        return TextInputParser::TryParsePositiveDouble(ReadEditText(edit, 64), value);
    }

    static bool ReadByteValue(HWND edit, int& value)
    {
        return TextInputParser::TryParseByte(ReadEditText(edit, 32), value);
    }

    static MeasurementUnit SelectedCalibrationUnit(HWND combo)
    {
        const LRESULT selection = combo ? SendMessageW(combo, CB_GETCURSEL, 0, 0) : 0;
        return CalibrationProfile::CalibrationUnitAtIndex(static_cast<int>(selection));
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
        InvalidatePreviewFrameCache();
        SetLatestFrameSource(L"");
        SetPreviewTelemetry(L"");
        image_viewport_.Reset();
        InvalidatePreview(hwnd_);
    }

    void CaptureThread()
    {
        const int requested_camera_index = selected_camera_index_;
        if (requested_camera_index < 0) {
            SetStatus(CameraControlStatusFormatter::FormatNoCameraSelected());
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
            SetStatus(CameraControlStatusFormatter::FormatNoCameraFound());
            running_ = false;
            return;
        }

        if (requested_camera_index >= static_cast<int>(devices.size())) {
            SetStatus(CameraControlStatusFormatter::FormatSelectedCameraUnavailable());
            running_ = false;
            return;
        }

        float requested_exposure = 10.0f;
        {
            std::lock_guard<std::mutex> lock(settings_mutex_);
            requested_exposure = requested_exposure_ms_;
        }
        if (!camera_driver_.Open(requested_camera_index, requested_exposure)) {
            SetStatus(CameraControlStatusFormatter::FormatFailedToOpenCamera());
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
        unsigned long long last_fps_sequence = 0;
        std::wstring final_status = CameraControlStatusFormatter::FormatPreviewStopped();

        while (running_) {
            ImageFrame frame;
            if (camera_driver_.GrabFrame(sequence + 1, frame)) {
                fail_count = 0;
                sequence = frame.sequence;
                const unsigned long timestamp = frame.timestamp;
                frame_buffer_.Publish(std::move(frame));
                PostMessageW(hwnd_, kMsgFrameReady, 0, 0);

                const DWORD now = GetTickCount();
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
                        final_status = CameraControlStatusFormatter::FormatCameraDisconnected();
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

    std::wstring BuildDiagnosticsReport()
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

        return DiagnosticReportActions::BuildReport(std::move(input), measurements_);
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

    std::mutex status_mutex_;
    std::mutex settings_mutex_;

    FrameBuffer frame_buffer_;
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
    float requested_exposure_ms_ = 10.0f;
    std::atomic_int camera_count_ = 0;
    std::atomic_int selected_camera_index_ = -1;
    ImageViewport image_viewport_;
    OverlayRenderer overlay_renderer_;
    PseudoColorPalette pseudo_color_palette_ = PseudoColorPalette::Original;
    std::vector<DyeProfile> dye_library_ = DyeLibrary::DefaultDyes();
    std::vector<FluorescenceChannel> fluorescence_channels_;
    std::vector<StitchTile> stitch_tiles_;
    int stitch_search_percent_ = ProcessingParameterRules::DefaultStitchSearchPercent();
    std::vector<ImageFrame> edf_stack_;
    ProcessingRetryState processing_retry_;
    ProcessingResultFrames processing_frames_;
    EdfOptions edf_options_ = ProcessingParameterRules::DefaultEdfOptions();
    ProcessingJobState processing_state_;
    CalibrationProfile calibration_ = CalibrationProfile::Uncalibrated();
    MeasurementCollection measurements_;
    MeasurementInteractionState measurement_interaction_;
    MeasurementEditSession edit_session_;
    double pending_calibration_length_ = 100.0;
    MeasurementUnit pending_calibration_unit_ = MeasurementUnit::Micrometers;
    bool show_fusion_preview_ = false;
    bool function_panel_visible_ = true;
    bool function_panel_docked_left_ = true;
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
            function_panel_docked_left);
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

        HWND device_combo = GetDlgItem(hwnd, kIdDeviceCombo);
        HWND calibration_unit = GetDlgItem(hwnd, kIdCalibrationUnitCombo);
        HWND pseudo_color_combo = GetDlgItem(hwnd, kIdPseudoColorCombo);
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

        for (const std::wstring& label_text : PreviewDisplayActions::PseudoColorLabels()) {
            SendMessageW(pseudo_color_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label_text.c_str()));
        }
        SendMessageW(pseudo_color_combo, CB_SETCURSEL, 0, 0);
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
        break;
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
        case kIdAddStitchTile:
            app->AddCurrentFrameAsStitchTile();
            return 0;
        case kIdBuildStitch:
            app->BuildStitchPreview();
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
    case WM_LBUTTONDOWN: {
        CameraPreviewApp* app = GetApp(hwnd);
        if (app) {
            POINT point = {
                static_cast<int>(static_cast<short>(LOWORD(lparam))),
                static_cast<int>(static_cast<short>(HIWORD(lparam)))
            };
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
            app->EndMeasurementEdit();
            app->EndPan();
        }
        break;
    }
    case kMsgFrameReady:
        if (CameraPreviewApp* app = GetApp(hwnd)) {
            app->InvalidatePreviewFrameCache();
        }
        InvalidatePreview(hwnd);
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

    HMENU main_menu = CreateMainMenu();
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

    ShowWindow(hwnd, show_command);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
