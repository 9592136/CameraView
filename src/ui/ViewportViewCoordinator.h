#pragma once

#include <windows.h>

#include "ControlIds.h"
#include "../i18n/Localization.h"

/// Aggregates WM_COMMAND dispatch for viewport-level and global commands:
/// layout toggles, docking, language, file i/o, fit-to-view, project, exit.
class ViewportViewCoordinator {
public:
    /// Dispatches a WM_COMMAND to the appropriate CameraPreviewApp method.
    /// Returns true when the command was recognised and handled.
    /// @tparam TApp  Type providing CameraPreviewApp's public viewport/global API.
    template <typename TApp>
    static bool DispatchCommand(TApp& app, HWND hwnd, WPARAM wparam, LPARAM /*lparam*/) {
        const int id = LOWORD(wparam);

        switch (id) {
        case kIdToggleFunctionPanel:
            app.ToggleFunctionPanel();
            return true;
        case kIdTogglePanelDock:
            app.ToggleFunctionPanelDock();
            return true;
        case kIdDockFunctionPanelLeft:
            app.SetFunctionPanelDockedLeft(true);
            return true;
        case kIdDockFunctionPanelRight:
            app.SetFunctionPanelDockedLeft(false);
            return true;
        case kIdLanguageEnglish:
            app.ReloadUILanguage(UILanguage::English);
            return true;
        case kIdLanguageChinese:
            app.ReloadUILanguage(UILanguage::Chinese);
            return true;
        case kIdAbout:
            app.ShowAboutDialog();
            return true;
        case kIdExit:
            DestroyWindow(hwnd);
            return true;
        case kIdFitView:
            app.FitView();
            return true;
        case kIdExportCsv:
            app.ExportMeasurementsCsv();
            return true;
        case kIdExportImage:
            app.ExportImage();
            return true;
        case kIdOpenImage:
            app.OpenImage();
            return true;
        case kIdSaveDiagnostics:
            app.SaveDiagnosticsReport();
            return true;
        case kIdDesignReportTemplate:
            app.ShowReportTemplateDesigner();
            return true;
        case kIdLoadReportTemplate:
            app.LoadReportTemplate();
            return true;
        case kIdClearReportTemplate:
            app.ClearReportTemplate();
            return true;
        case kIdOpenProject:
            app.OpenProject();
            return true;
        case kIdSaveProject:
            app.SaveProject();
            return true;
        default:
            return false;
        }
    }
};
