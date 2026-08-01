#include "ui/CameraPreviewApp.h"
#include "ui/LayoutUtils.h"
#include "ui/MainMenu.h"
#include "ui/WindowProperties.h"

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE: {
        auto app = std::make_unique<CameraPreviewApp>(hwnd);
        CameraPreviewApp* app_ptr = app.get();
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app_ptr));
        app.release();  // ownership transferred to GWLP_USERDATA, freed in WM_DESTROY
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
        app_ptr->InitializeObjectiveControls(objective_combo, objective_name_edit);

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

        app_ptr->SyncPanelCardButtons();
        app_ptr->SyncFunctionPanelChrome();
        app_ptr->InitializeDyeCombo(dye_combo);
        app_ptr->SyncSelectedDyeControls(
            dye_combo,
            dye_name_edit,
            dye_excitation_edit,
            dye_emission_edit,
            dye_red_edit,
            dye_green_edit,
            dye_blue_edit);
        app_ptr->InitializeStitchControls();

        LayoutControls(hwnd);
        if (app_ptr->RefreshCameraList(device_combo)) {
            app_ptr->Start();
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
        if (!app) break;

        const int panel_category = WindowControlLayout::PanelCategoryFromCardControl(LOWORD(wparam));
        if (panel_category >= 0) {
            app->ShowPanelCategory(panel_category);
            LayoutControls(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        // Delegated to panel-domain coordinators (in priority order).
        if (CameraViewCoordinator::DispatchCommand       (*app, hwnd, wparam, lparam)) return 0;
        if (FluorescenceViewCoordinator::DispatchCommand  (*app, hwnd, wparam, lparam)) return 0;
        if (MeasurementViewCoordinator::DispatchCommand   (*app, hwnd, wparam, lparam)) return 0;
        if (ProcessingViewCoordinator::DispatchCommand    (*app, hwnd, wparam, lparam)) return 0;
        if (ViewportViewCoordinator::DispatchCommand      (*app, hwnd, wparam, lparam)) return 0;

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
