#pragma once

#include <windows.h>

#include "ControlIds.h"

/// Aggregates WM_COMMAND dispatch for the Measurement panel
/// (tool selection, polygon finish, delete / rename / clear).
class MeasurementViewCoordinator {
public:
    /// Dispatches a WM_COMMAND to the appropriate CameraPreviewApp method.
    /// Returns true when the command was recognised and handled.
    /// @tparam TApp  Type providing CameraPreviewApp's public measurement API.
    template <typename TApp>
    static bool DispatchCommand(TApp& app, HWND hwnd, WPARAM wparam, LPARAM /*lparam*/) {
        const int id   = LOWORD(wparam);
        const int code = HIWORD(wparam);

        if (id == kIdResultsList && code == LBN_SELCHANGE) {
            app.SyncSelectedMeasurementName(
                GetDlgItem(hwnd, kIdResultsList),
                GetDlgItem(hwnd, kIdMeasurementNameEdit));
            return true;
        }

        switch (id) {
        case kIdLengthTool:
            app.BeginLengthMeasurement();
            return true;
        case kIdAngleTool:
            app.BeginAngleMeasurement();
            return true;
        case kIdRectangleAreaTool:
            app.BeginRectangleAreaMeasurement();
            return true;
        case kIdPolygonAreaTool:
            app.BeginPolygonAreaMeasurement();
            return true;
        case kIdFinishPolygonArea:
            app.FinishPolygonAreaMeasurement();
            return true;
        case kIdDeleteMeasurement:
            app.DeleteSelectedMeasurement(GetDlgItem(hwnd, kIdResultsList));
            return true;
        case kIdRenameMeasurement:
            app.RenameSelectedMeasurement(
                GetDlgItem(hwnd, kIdResultsList),
                GetDlgItem(hwnd, kIdMeasurementNameEdit));
            return true;
        case kIdClearMeasurements:
            app.ClearMeasurements(GetDlgItem(hwnd, kIdResultsList));
            return true;
        default:
            return false;
        }
    }
};
