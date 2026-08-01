#pragma once

#include <windows.h>

#include "ControlIds.h"

/// Aggregates WM_COMMAND dispatch for the Camera panel (device, exposure,
/// white-balance, calibration, objectives), keeping the WindowProc thin.
class CameraViewCoordinator {
public:
    /// Dispatches a WM_COMMAND to the appropriate CameraPreviewApp method.
    /// Returns true when the command was recognised and handled.
    /// @tparam TApp  Type providing CameraPreviewApp's public camera API
    ///               (instantiated in main.cpp where the class is complete).
    template <typename TApp>
    static bool DispatchCommand(TApp& app, HWND hwnd, WPARAM wparam, LPARAM /*lparam*/) {
        const int id   = LOWORD(wparam);
        const int code = HIWORD(wparam);

        if (id == kIdDeviceCombo && code == CBN_SELCHANGE) {
            app.UpdateSelectedCamera(GetDlgItem(hwnd, kIdDeviceCombo));
            return true;
        }
        if (id == kIdObjectiveCombo && code == CBN_SELCHANGE) {
            app.SelectObjective(GetDlgItem(hwnd, kIdObjectiveCombo),
                                GetDlgItem(hwnd, kIdObjectiveNameEdit));
            return true;
        }

        switch (id) {
        case kIdOpen:
            app.UpdateSelectedCamera(GetDlgItem(hwnd, kIdDeviceCombo));
            app.Start();
            return true;
        case kIdStop:
            app.Stop();
            InvalidatePreview(hwnd);
            return true;
        case kIdRefreshDevices:
            app.RefreshCameraList(GetDlgItem(hwnd, kIdDeviceCombo));
            InvalidatePreview(hwnd);
            InvalidateStatus(hwnd);
            return true;
        case kIdExposureApply:
            app.ApplyExposure(GetDlgItem(hwnd, kIdExposureEdit));
            return true;
        case kIdAutoExposure:
            app.ApplyAutoExposure();
            return true;
        case kIdCameraExposureApply:
            app.ApplyExposure(GetDlgItem(hwnd, kIdCameraExposureEdit));
            return true;
        case kIdCameraGainApply:
            app.ApplyGain(GetDlgItem(hwnd, kIdCameraGainEdit));
            return true;
        case kIdWhiteBalance:
            app.ApplyWhiteBalance();
            return true;
        case kIdCalibrate:
            app.BeginCalibration(GetDlgItem(hwnd, kIdCalibrationLengthEdit),
                                 GetDlgItem(hwnd, kIdCalibrationUnitCombo));
            return true;
        case kIdClearCalibration:
            app.ClearCalibration();
            return true;
        case kIdAddObjective:
            app.AddObjective(GetDlgItem(hwnd, kIdObjectiveCombo),
                             GetDlgItem(hwnd, kIdObjectiveNameEdit));
            return true;
        case kIdRenameObjective:
            app.RenameSelectedObjective(GetDlgItem(hwnd, kIdObjectiveCombo),
                                        GetDlgItem(hwnd, kIdObjectiveNameEdit));
            return true;
        case kIdDeleteObjective:
            app.DeleteSelectedObjective(GetDlgItem(hwnd, kIdObjectiveCombo),
                                        GetDlgItem(hwnd, kIdObjectiveNameEdit));
            return true;
        default:
            return false;
        }
    }
};
